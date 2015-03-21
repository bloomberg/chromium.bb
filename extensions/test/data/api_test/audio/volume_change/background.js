// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.audio.OnLevelChanged.addListener(function (id, level) {
  if (id == 30001 && level == 60) {
    chrome.test.sendMessage("success");
  } else {
    console.error("Got unexpected OnLevelChanged event id:" + id +
                  " level:" + level);
    chrome.test.sendMessage("failure");
  }
});
chrome.test.sendMessage("loaded");
