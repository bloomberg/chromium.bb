// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns a function that throws a 'not available' exception when called.
 *
 * @param {string} messagePrefix text to prepend to the exception message.
 */
function generateStub(messagePrefix) {
  return function() {
    throw messagePrefix + ' is not available in packaged apps.';
  };
}

/**
 * Replaces the given methods of the passed in object with stubs that throw
 * 'not available' exceptions when called.
 *
 * @param {Object} object The object whose methods to stub out. The prototype
 *     is preferred.
 * @param {string} objectName The display name to use in the error message
 *     thrown by the stub (this is the name that the object is commonly referred
 *     to by web developers, e.g. "document" instead of "HTMLDocument").
 * @param {Array.<string>} methodNames method names
 */
function stubOutMethods(object, objectName, methodNames) {
  methodNames.forEach(function(methodName) {
    object[methodName] = generateStub(objectName + '.' + methodName + '()');
  });
}

/**
 * Replaces the given properties of the passed in object with stubs that throw
 * 'not available' exceptions when gotten.
 *
 * @param {Object} object The object whose properties to stub out. The prototype
 *     is preferred.
 * @param {string} objectName The display name to use in the error message
 *     thrown by the stub (this is the name that the object is commonly referred
 *     to by web developers, e.g. "document" instead of "HTMLDocument").
 * @param {Array.<string>} propertyNames property names
 */
function stubOutGetters(object, objectName, propertyNames) {
  propertyNames.forEach(function(propertyName) {
    object.__defineGetter__(
        propertyName, generateStub(objectName + '.' + propertyName));
  });
}

// Disable document.open|close|write|etc.
stubOutMethods(HTMLDocument.prototype, 'document',
    ['open', 'clear', 'close', 'write', 'writeln']);

// Deprecated document properties from
// https://developer.mozilla.org/en/DOM/document.
stubOutGetters(document, 'document',
    ['alinkColor', 'all', 'bgColor', 'fgColor', 'linkColor', 'vlinkColor']);

// Disable history.
window.history = {};
stubOutMethods(window.history, 'history',
    ['back', 'forward', 'go', 'pushState', 'replaceState']);
stubOutGetters(window.history, 'history', ['length', 'state']);

// Disable find.
stubOutMethods(Window.prototype, 'window', ['find']);

// Disable modal dialogs. Shell windows disable these anyway, but it's nice to
// warn.
stubOutMethods(Window.prototype, 'window', ['alert', 'confirm', 'prompt']);

// Disable window.*bar.
stubOutGetters(window, 'window',
    ['locationbar', 'menubar', 'personalbar', 'scrollbars', 'statusbar',
    'toolbar']);

// Disable onunload, onbeforeunload.
Window.prototype.__defineSetter__(
    'onbeforeunload', generateStub('onbeforeunload'));
Window.prototype.__defineSetter__('onunload', generateStub('onunload'));
var windowAddEventListener = Window.prototype.addEventListener;
Window.prototype.addEventListener = function(type) {
  if (type === 'unload' || type === 'beforeunload')
    generateStub(type)();
  else
    return windowAddEventListener.apply(window, arguments);
};
