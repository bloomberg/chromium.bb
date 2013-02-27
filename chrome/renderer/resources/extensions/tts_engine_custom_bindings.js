// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the ttsEngine API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.Event.registerArgumentMassager('ttsEngine.onSpeak',
    function(args, dispatch) {
  var text = args[0];
  var options = args[1];
  var requestId = args[2];
  var sendTtsEvent = function(event) {
    chrome.ttsEngine.sendTtsEvent(requestId, event);
  };
  dispatch([text, options, sendTtsEvent]);
});
