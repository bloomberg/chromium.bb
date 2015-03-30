// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.audio.OnMuteChanged.addListener(function (is_input, is_muted) {
  if (is_input === false && is_muted === false) {
    chrome.test.sendMessage("success");
  } else {
    console.error("Got unexpected OnMuteChanged event is_input:" + is_input +
                  " is_muted:" + is_muted);
    chrome.test.sendMessage("failure");
  }
});
chrome.test.sendMessage("loaded");
