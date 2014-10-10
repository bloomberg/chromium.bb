// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DocumentNatives = requireNative('document_natives');

// Output error message to console when using the <webview> tag with no
// permission.
var errorMessage = "You do not have permission to use the appview element." +
  " Be sure to declare the 'appview' permission in your manifest file and use" +
  " the --enable-app-view command line flag.";

// Registers <webview> custom element.
function registerAppViewElement() {
  var proto = Object.create(HTMLElement.prototype);

  proto.createdCallback = function() {
    window.console.error(errorMessage);
  };

  window.AppView =
      DocumentNatives.RegisterElement('appview', {prototype: proto});

  // Delete the callbacks so developers cannot call them and produce unexpected
  // behavior.
  delete proto.createdCallback;
  delete proto.attachedCallback;
  delete proto.detachedCallback;
  delete proto.attributeChangedCallback;
}

var useCapture = true;
window.addEventListener('readystatechange', function listener(event) {
  if (document.readyState == 'loading')
    return;

  registerAppViewElement();
  window.removeEventListener(event.type, listener, useCapture);
}, useCapture);
