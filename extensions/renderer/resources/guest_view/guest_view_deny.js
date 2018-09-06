// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the registration of guestview elements when
// permissions are not available. These elements exist only to provide a useful
// error message when developers attempt to use them.

var DocumentNatives = requireNative('document_natives');

var ERROR_MESSAGE = 'You do not have permission to use the %1 element.' +
    ' Be sure to declare the "%1" permission in your manifest file.';

// A list of view types that will have custom elements registered if they are
// not already registered by the time this module is loaded.
var VIEW_TYPES = [
  'AppView',
  'ExtensionOptions',
  'ExtensionView',
  'WebView'
];

// Registers a GuestView custom element.
function registerGuestViewElement(viewType) {
  var DeniedElement = class extends HTMLElement {}

  // We set the lifecycle callback so that it's available during
  // registration. Once that's done, we'll delete it so developers cannot
  // call it.
  DeniedElement.prototype.createdCallback = function() {
    window.console.error(
        $String.replace(ERROR_MESSAGE, /%1/g, $String.toLowerCase(viewType)));
  };

  window[viewType] = DocumentNatives.RegisterElement(
      $String.toLowerCase(viewType), {prototype: DeniedElement.prototype});

  delete DeniedElement.prototype.createdCallback;
}

var useCapture = true;
window.addEventListener('readystatechange', function listener(event) {
  if (document.readyState == 'loading')
    return;

  for (var viewType of VIEW_TYPES) {
    // Register the error-providing custom element only for those view types
    // that have not already been registered. Since this module is always loaded
    // last, all the view types that are available (i.e. have the proper
    // permissions) will have already been registered on |window|.
    if (!window[viewType])
      registerGuestViewElement(viewType);
  }

  window.removeEventListener(event.type, listener, useCapture);
}, useCapture);
