// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('__crWeb.dialogOverrides');

goog.require('__crWeb.core');

__gCrWeb.dialogOverrides = {};

(function() {
  /*
   * Installs a wrapper around geolocation functions displaying dialogs in order
   * to control their suppression.
   *
   * Since the Javascript on the page may cache the value of those functions
   * and invoke them later, we must only install the wrapper once and change
   * their behaviour when required.
   *
   * Returns a function that allows changing the value of
   * |suppressGeolocationDialogs| that is tested by the wrappers.
   */
  var installGeolocationDialogOverridesMethods = function() {
    var suppressGeolocationDialogs = false;

    // Returns a wrapper function around original dialog function. The wrapper
    // may suppress the dialog and notify host.
    var makeDialogWrapper = function(originalDialogGetter) {
      return function() {
        if (suppressGeolocationDialogs) {
          __gCrWeb.message.invokeOnHost({
              'command': 'geolocationDialog.suppressed'
          });
        } else {
          return originalDialogGetter().apply(null, arguments);
        }
      };
    };

    // Reading or writing to the property 'geolocation' too early breaks
    // the API. Make a copy of navigator and stub in the required methods
    // without touching the property. See crbug.com/280818 for more
    // details.
    var stubNavigator = {};

    // Copy all properties and functions without touching 'geolocation'.
    var oldNavigator = navigator;
    for (var keyName in navigator) {
      if (keyName !== 'geolocation') {
        var value = navigator[keyName];
        if (typeof(value) == 'function') {
          // Forward functions calls to real navigator.
          stubNavigator[keyName] = function() {
            return value.apply(oldNavigator, arguments);
          }
        } else {
          Object['defineProperty'](stubNavigator, keyName, {
            value: value,
            configurable: false,
            writable: false,
            enumerable: true
          });
        }
      }
    }

    // Stub in 'geolocation' if necessary, using delayed accessor for the
    // 'geolocation' property of the original |navigator|.
    if ('geolocation' in navigator) {
      var geolocation = {};
      var geoPropNames = ['getCurrentPosition', 'watchPosition', 'clearWatch'];
      var len = geoPropNames.length;
      for (var i = 0; i < len; i++) {
        (function(geoPropName) {
          geolocation[geoPropName] = makeDialogWrapper(function() {
             return function() {
               return oldNavigator.geolocation[geoPropName].apply(
                   oldNavigator.geolocation, arguments);
             };
          });
        })(geoPropNames[i]);
      }
      stubNavigator.geolocation = geolocation;
    }

    // Install |stubNavigator| as |navigator|.
    /** @suppress {const} */
    navigator = stubNavigator;

    // Returns the closure allowing to change |suppressGeolocationDialogs|.
    return function(setEnabled) {
      suppressGeolocationDialogs = setEnabled;
    };
  };

  // Override geolocation methods that produce dialogs.
  __gCrWeb['setSuppressGeolocationDialogs'] =
      installGeolocationDialogOverridesMethods();

}());  // End of anonymous object
