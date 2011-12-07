// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testExtensionApi() {
  try {
    chrome.tabs.getAllInWindow(null, function() {
      window.domAutomationController.send(
          !chrome.extension.lastError);
    });
  } catch (e) {
    window.domAutomationController.send(false);
  }
}
