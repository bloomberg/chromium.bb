// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('__crWeb.dialogOverrides');

goog.require('__crWeb.core');
// window.open override for dialog suppression must be performed after the rest
// of window.open overrides which are done in __crWeb.windowOpen module.
goog.require('__crWeb.windowOpen');

// Namespace for this module.
__gCrWeb.dialogOverrides = {};

// Beginning of anonymous object.
(function() {
  /*
   * Install a wrapper around functions displaying dialogs in order to catch
   * code displaying dialog.
   *
   * Since the Javascript on the page may cache the value of those functions
   * and invoke them later, we must only install the wrapper once and change
   * their behaviour when required.
   *
   * Returns a function that allows changing the value of the two booleans
   * |suppressDialogs| and |notifyAboutDialogs| that are tested by the wrappers.
   */
  var installDialogOverridesMethods = function() {
    var suppressDialogs = false;
    var notifyAboutDialogs = false;

    // Returns a wrapper function around |originalDialog|. The wrapper may
    // suppress the dialog and notify host about show/suppress.
    var makeDialogWrapper = function(originalDialogGetter) {
      return function() {
        if (!suppressDialogs) {
          if (notifyAboutDialogs) {
            __gCrWeb.message.invokeOnHost({'command': 'dialog.willShow'});
          }
          return originalDialogGetter().apply(null, arguments);
        } else if (notifyAboutDialogs) {
          __gCrWeb.message.invokeOnHost({'command': 'dialog.suppressed'});
        }
      };
    };

    // Install wrapper around the following properties of |window|.
    var wrappedFunctionNames = ['alert', 'confirm', 'prompt', 'open'];
    var len = wrappedFunctionNames.length;
    for (var i = 0; i < len; i++) {
      (function(wrappedFunctionName) {
        var wrappedDialogMethod = window[wrappedFunctionName];
        window[wrappedFunctionName] = makeDialogWrapper(
          function() { return wrappedDialogMethod; });
      })(wrappedFunctionNames[i]);
    }

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

    // Returns the closure allowing to change |suppressDialogs| and
    // |notifyAboutDialogs| variables.
    return function(setEnabled, setNotify) {
      suppressDialogs = setEnabled;
      notifyAboutDialogs = setNotify;
    };
  };

  // Override certain methods that produce dialogs. This needs to be installed
  // after other window methods overrides.
  __gCrWeb['setSuppressDialogs'] = installDialogOverridesMethods();

}());  // End of anonymous object
