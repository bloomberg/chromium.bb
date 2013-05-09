// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple success test: we want content-script APIs to be available (like
// sendRequest), but other APIs to be undefined or throw exceptions on access.

var success = true;

// The whole of chrome.storage (arbitrary unprivileged) is unavailable.
if (chrome.storage) {
  console.log('Error: chrome.storage exists, it shouldn\'t.');
  success = false;
}

// Ditto chrome.tabs, though it's special because it's a dependency of the
// partially unprivileged chrome.extension.
if (chrome.tabs) {
  console.log('Error: chrome.tabs exists, it shouldn\'t.');
  success = false;
}

// Parts of chrome.extension are unavailable.
if (typeof(chrome.extension.getViews) != 'undefined')
  success = false;

chrome.extension.sendRequest({success: success});
