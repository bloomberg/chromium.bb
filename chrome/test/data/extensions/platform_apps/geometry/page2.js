// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkGeometry() {
  chrome.test.assertEq(137, window.screenX);
  chrome.test.assertEq(143, window.screenY);
  chrome.test.assertEq(203, window.innerWidth);
  chrome.test.assertEq(187, window.innerHeight);
  chrome.test.sendMessage('Done2');
}

window.setTimeout(checkGeometry, 500);
