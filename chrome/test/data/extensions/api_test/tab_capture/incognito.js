// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabCapture.capture({audio: true, video: true}, function(stream) {
  chrome.test.assertTrue(!!stream);
  chrome.test.succeed();
});
