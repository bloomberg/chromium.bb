// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple success test: we want content-script APIs to be available (like
// sendRequest), but other APIs to be undefined or throw exceptions on access.

function runsWithException(f) {
  try {
    var foo = f();
    console.log('Error: ' + f + '" doesn\'t throw exception.');
    return false;
  } catch (e) {
    if (e.message.indexOf(' can only be used in extension processes.') > -1) {
      return true;
    } else {
      console.log('Error: incorrect exception message: ' + e.message);
      return false;
    }
  }
}

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
if (!runsWithException(function() { return chrome.extension.getViews; }))
  success = false;

chrome.extension.sendRequest({success: success});
