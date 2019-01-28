// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener((request, from, sendResponse) => {
  if (request.ping) {
    console.info("Got ping, sending pong...");
    sendResponse({pong: true});
  }
});
