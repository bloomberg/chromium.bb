// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the tts API.

var ttsNatives = requireNative('tts');
var GetNextTTSEventId = ttsNatives.GetNextTTSEventId;

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;
var lazyBG = requireNative('lazy_background_page');

chromeHidden.registerCustomHook('tts', function(api) {
  var apiFunctions = api.apiFunctions;

  chromeHidden.tts = {};
  chromeHidden.tts.handlers = {};
  chrome.tts.onEvent.addListener(function(event) {
    var eventHandler = chromeHidden.tts.handlers[event.srcId];
    if (eventHandler) {
      eventHandler({
                     type: event.type,
                     charIndex: event.charIndex,
                     errorMessage: event.errorMessage
                   });
      if (event.isFinalEvent) {
        delete chromeHidden.tts.handlers[event.srcId];
        // Balanced in 'speak' handler.
        lazyBG.DecrementKeepaliveCount();
      }
    }
  });

  apiFunctions.setHandleRequest('speak', function() {
    var args = arguments;
    if (args.length > 1 && args[1] && args[1].onEvent) {
      var id = GetNextTTSEventId();
      args[1].srcId = id;
      chromeHidden.tts.handlers[id] = args[1].onEvent;
      // Keep the page alive until the event finishes.
      // Balanced in eventHandler.
      lazyBG.IncrementKeepaliveCount();
    }
    sendRequest(this.name, args, this.definition.parameters);
    return id;
  });
});
