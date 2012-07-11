// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the input ime API. Only injected into the
// v8 contexts for extensions which have permission for the API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('input.ime', function() {
  chrome.input.ime.onKeyEvent.dispatchToListener = function(callback, args) {
    var engineID = args[0];
    var keyData = args[1];

    var result = false;
    try {
      result = chrome.Event.prototype.dispatchToListener(callback, args);
    } catch (e) {
      console.error('Error in event handler for onKeyEvent: ' + e.stack);
    }
    chrome.input.ime.eventHandled(keyData.requestId, result);
  };
});
