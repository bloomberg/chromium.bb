// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  // Passing super-large messages should be prevented by the renderer.
  var tooLarge = 1024 * 1024 * 128;
  chrome.runtime.sendMessage('a'.repeat(tooLarge));
  chrome.test.notifyFail();
} catch (e) {
  chrome.test.assertEq("Message length exceeded maximum allowed length.",
                       e.message);
  chrome.test.notifyPass();
}
