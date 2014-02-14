// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called when the user activates the command.
chrome.commands.onCommand.addListener(function(command) {
  chrome.tabs.executeScript(null, {
      code: "document.body.bgColor='" + command + "'" });
  chrome.test.notifyPass();
});

chrome.test.notifyPass();
