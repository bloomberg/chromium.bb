// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g_infobarReady = false;
var g_callback = null;
function waitForInfobar(callback) {
  if (g_infobarReady)
    callback();
  g_callback = callback;
}

chrome.tabs.getSelected(null, function(tab) {
  chrome.experimental.infobars.show(
      {'tabId': tab.id, 'path': 'view_checks.html'}, function() {
    g_infobarReady = true;
    if (g_callback)
      g_callback();
  });
});
