// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the ttsEngine API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('ttsEngine', function() {
  chrome.ttsEngine.onSpeak.dispatch = function(text, options, requestId) {
    var sendTtsEvent = function(event) {
      chrome.ttsEngine.sendTtsEvent(requestId, event);
    };
    chrome.Event.prototype.dispatch.apply(
        this, [text, options, sendTtsEvent]);
  };
});
