// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the input ime API. Only injected into the
// v8 contexts for extensions which have permission for the API.

var binding = require('binding').Binding.create('input.ime');

binding.registerCustomHook(function(api) {
  var input_ime = api.compiledApi;

  input_ime.onKeyEvent.dispatchToListener = function(callback, args) {
    var engineID = args[0];
    var keyData = args[1];

    var result = false;
    try {
      result = chrome.Event.prototype.dispatchToListener(callback, args);
    } catch (e) {
      console.error('Error in event handler for onKeyEvent: ' + e.stack);
    }
    if (!input_ime.onKeyEvent.async)
      input_ime.keyEventHandled(keyData.requestId, result);
  };

  input_ime.onKeyEvent.addListener = function(cb, opt_extraInfo) {
    input_ime.onKeyEvent.async = false;
    if (opt_extraInfo instanceof Array) {
      for (var i = 0; i < opt_extraInfo.length; ++i) {
        if (opt_extraInfo[i] == "async") {
          input_ime.onKeyEvent.async = true;
        }
      }
    }
    chrome.Event.prototype.addListener.call(this, cb, null);
  };
});

exports.binding = binding.generate();
