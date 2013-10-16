// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(function(message, sender, reply) {
  if (message[0] == "getStream") {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"],
        sender.tab,
        function(id) {
          reply({"id": id});
        });
    return true;
  } else {
    reply({"error": "invalid message"});
  }
});
