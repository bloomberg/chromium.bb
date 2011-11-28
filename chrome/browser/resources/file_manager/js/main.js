// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  FileManager.initStrings(function () {
    metrics.startInterval('Load.Construct');
    fileManager = new FileManager(document.body);
    metrics.recordTime('Load.Construct');
    // We're ready to run.  Tests can monitor for this state with
    // ExtensionTestMessageListener listener("ready");
    // ASSERT_TRUE(listener.WaitUntilSatisfied());
    chrome.test.sendMessage('ready');
  });
}
