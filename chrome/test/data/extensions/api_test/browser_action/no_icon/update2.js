// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that setting an icon using its relative path works.
chrome.browserAction.setIcon({path: "icon.png"});
chrome.test.notifyPass();
