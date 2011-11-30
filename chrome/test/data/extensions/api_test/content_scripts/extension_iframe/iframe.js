// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple success test: we want content-script APIs to be available (like
// sendRequest), but other APIs to be undefined.
var success = false;

try {
  var foo = chrome.tabs.create;
} catch (e) {
  success = e.message.indexOf(
      " can only be used in extension processes.") > -1;
}

chrome.extension.sendRequest({success: success});
