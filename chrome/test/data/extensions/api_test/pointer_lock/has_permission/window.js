// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function () {
  chrome.test.runTests([
    function requestPointerLock() {
      document.onwebkitpointerlockchange = chrome.test.succeed;
      document.onwebkitpointerlockerror = chrome.test.fail;
      document.body.webkitRequestPointerLock();
    },
  ]);
}
