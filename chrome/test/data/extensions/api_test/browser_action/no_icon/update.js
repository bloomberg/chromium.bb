// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that even if we set the icon after the extension loads, it shows up.
chrome.browserAction.setIcon({imageData:document.getElementById("canvas")
    .getContext('2d').getImageData(0, 0, 16, 16)});
chrome.test.notifyPass();
