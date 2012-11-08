// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkGeometry() {
  chrome.test.assertEq(113, window.screenX);
  chrome.test.assertEq(117, window.screenY);
  chrome.test.assertEq(314, window.innerWidth);
  chrome.test.assertEq(271, window.innerHeight);
  chrome.test.sendMessage('Done1');
}

window.setTimeout(checkGeometry, 500);
