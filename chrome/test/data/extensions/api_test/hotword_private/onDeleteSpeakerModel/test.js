// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function onDeleteSpeakerModel() {
    chrome.hotwordPrivate.onDeleteSpeakerModel.addListener(function () {
      chrome.test.sendMessage("notification");
      chrome.test.succeed();
    });
    chrome.hotwordPrivate.getAudioHistoryEnabled(
        chrome.test.callbackPass(function(state) {}));
    chrome.test.sendMessage("ready");
  }
]);
