// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns a function that throws a 'not available' exception when called.
 *
 * @param {string} messagePrefix text to prepend to the exception message.
 */
function generateDisabledMethodStub(messagePrefix, opt_messageSuffix) {
  return function() {
    var message = messagePrefix + ' is not available in packaged apps.';
    if (opt_messageSuffix) message = message + ' ' + opt_messageSuffix;
    throw message;
  };
}

/**
 * Replaces the given methods of the passed in object with stubs that throw
 * 'not available' exceptions when called.
 *
 * @param {Object} object The object with methods to disable. The prototype is
 *     preferred.
 * @param {string} objectName The display name to use in the error message
 *     thrown by the stub (this is the name that the object is commonly referred
 *     to by web developers, e.g. "document" instead of "HTMLDocument").
 * @param {Array.<string>} methodNames names of methods to disable.
 */
function disableMethods(object, objectName, methodNames) {
  methodNames.forEach(function(methodName) {
    object[methodName] =
        generateDisabledMethodStub(objectName + '.' + methodName + '()');
  });
}

/**
 * Replaces the given properties of the passed in object with stubs that throw
 * 'not available' exceptions when gotten.  If a property's setter is later
 * invoked, the getter and setter are restored to default behaviors.
 *
 * @param {Object} object The object with properties to disable. The prototype
 *     is preferred.
 * @param {string} objectName The display name to use in the error message
 *     thrown by the getter stub (this is the name that the object is commonly
 *     referred to by web developers, e.g. "document" instead of
 *     "HTMLDocument").
 * @param {Array.<string>} propertyNames names of properties to disable.
 */
function disableGetters(object, objectName, propertyNames, opt_messageSuffix) {
  propertyNames.forEach(function(propertyName) {
    var stub = generateDisabledMethodStub(objectName + '.' + propertyName,
                                          opt_messageSuffix);
    stub._is_platform_app_disabled_getter = true;
    object.__defineGetter__(propertyName, stub);

    object.__defineSetter__(propertyName, function(value) {
      var getter = this.__lookupGetter__(propertyName);
      if (!getter || getter._is_platform_app_disabled_getter) {
        // The stub getter is still defined.  Blow-away the property to restore
        // default getter/setter behaviors and re-create it with the given
        // value.
        delete this[propertyName];
        this[propertyName] = value;
      } else {
        // Do nothing.  If some custom getter (not ours) has been defined, there
        // would be no way to read back the value stored by a default setter.
        // Also, the only way to clear a custom getter is to first delete the
        // property.  Therefore, the value we have here should just go into a
        // black hole.
      }
    });
  });
}

// Disable document.open|close|write|etc.
disableMethods(HTMLDocument.prototype, 'document',
    ['open', 'clear', 'close', 'write', 'writeln']);

// Disable history.
window.history = {};
disableMethods(window.history, 'history',
    ['back', 'forward', 'go']);
disableGetters(window.history, 'history', ['length']);

// Disable find.
disableMethods(Window.prototype, 'window', ['find']);

// Disable modal dialogs. Shell windows disable these anyway, but it's nice to
// warn.
disableMethods(Window.prototype, 'window', ['alert', 'confirm', 'prompt']);

// Disable window.*bar.
disableGetters(window, 'window',
    ['locationbar', 'menubar', 'personalbar', 'scrollbars', 'statusbar',
     'toolbar']);

// Disable window.localStorage.
disableGetters(window, 'window',
    ['localStorage'],
    'Use chrome.storage.local instead.');

// Document instance properties that we wish to disable need to be set when
// the document begins loading, since only then will the "document" reference
// point to the page's document (it will be reset between now and then).
// We can't listen for the "readystatechange" event on the document (because
// the object that it's dispatched on doesn't exist yet), but we can instead
// do it at the window level in the capturing phase.
window.addEventListener('readystatechange', function(event) {
  if (document.readyState != 'loading')
    return;

  // Deprecated document properties from
  // https://developer.mozilla.org/en/DOM/document.
  disableGetters(document, 'document',
      ['alinkColor', 'all', 'bgColor', 'fgColor', 'linkColor',
       'vlinkColor']);
}, true);

// Disable onunload, onbeforeunload.
Window.prototype.__defineSetter__(
    'onbeforeunload', generateDisabledMethodStub('onbeforeunload'));
Window.prototype.__defineSetter__(
    'onunload', generateDisabledMethodStub('onunload'));
var windowAddEventListener = Window.prototype.addEventListener;
Window.prototype.addEventListener = function(type) {
  if (type === 'unload' || type === 'beforeunload')
    generateDisabledMethodStub(type)();
  else
    return windowAddEventListener.apply(window, arguments);
};
