// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function audioHistory() {
    chrome.hotwordPrivate.setAudioHistoryEnabled(
        true,
        chrome.test.callbackPass(function(state) {
          if (state.success)
            chrome.test.sendMessage("set AH: "+state.enabled+" success");
          else
            chrome.test.sendMessage("set AH: "+state.enabled+" failure");
        }));
    // Test with setting to false as well.
    chrome.hotwordPrivate.setAudioHistoryEnabled(
        false,
        chrome.test.callbackPass(function(state) {
          if (state.success)
            chrome.test.sendMessage("set AH: "+state.enabled+" success");
          else
            chrome.test.sendMessage("set AH: "+state.enabled+" failure");
        }));
    chrome.hotwordPrivate.getAudioHistoryEnabled(chrome.test.callbackPass(
        function(state) {
          if (state.success)
            chrome.test.sendMessage("get AH: "+state.enabled+" success");
          else
            chrome.test.sendMessage("get AH: "+state.enabled+" failure");
        }));
  }
]);
