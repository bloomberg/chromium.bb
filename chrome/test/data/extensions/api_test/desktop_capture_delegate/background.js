// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(function(message, sender, reply) {
  if (message[0] == "getStream") {
    origin = message[1];
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"],
        origin,
        function(id) {
          reply({"id": id});
        });
    return true;
  } else {
    reply({"error": "invalid message"});
  }
});
