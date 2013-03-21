// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Global fileManager reference useful for poking at from the console.
 */
var fileManager;

/**
 * Kick off the file manager dialog.
 *
 * Called by main.html after the dom has been parsed.
 */
function init() {
  // Load test harness when running outside Chrome OS and not in an iframe.
  if (chrome.fileBrowserPrivate.mocked &&
      window == window.top && !window.harness) {
    util.loadScripts(['js/harness.js'], function() {
      harness.initEmbedded(init);
    });
    return;
  }

  FileManager.initStrings(function() {
    metrics.startInterval('Load.Construct');
    fileManager = new FileManager(document.body);
    metrics.recordInterval('Load.Construct');
    // We're ready to run.  Tests can monitor for this state with
    // ExtensionTestMessageListener listener("ready");
    // ASSERT_TRUE(listener.WaitUntilSatisfied());
    chrome.test.sendMessage('ready');
  });
}

util.addPageLoadHandler(init);

metrics.recordInterval('Load.Script');  // Must be the last line.
