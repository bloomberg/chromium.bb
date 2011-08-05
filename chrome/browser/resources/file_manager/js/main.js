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
  var rootPaths = ['Downloads', 'removable', 'archive', 'tmp'];

  function onEntriesFound(filesystem, entries) {
    FileManager.initStrings(function () {
      fileManager = new FileManager(document.body, filesystem, entries);
      // We're ready to run.  Tests can monitor for this state with
      // ExtensionTestMessageListener listener("ready");
      // ASSERT_TRUE(listener.WaitUntilSatisfied());
      chrome.test.sendMessage('ready');
    });
  }

  function onFileSystemFound(filesystem) {
    console.log('Found filesystem: ' + filesystem.name, filesystem);

    var entries = [];

    function onPathError(path, err) {
      console.error('Error locating root path: ' + path + ': ' + err);
    }

    function onEntryFound(entry) {
      if (entry) {
        entries.push(entry);
      } else {
        onEntriesFound(filesystem, entries);
      }
    }

    if (filesystem.name.match(/^chrome-extension_\S+:external/i)) {
      // We've been handed the local filesystem, whose root directory
      // cannot be enumerated.
      util.getDirectories(filesystem.root, {create: false}, rootPaths,
                          onEntryFound, onPathError);
    } else {
      util.forEachDirEntry(filesystem.root, onEntryFound);
    }
  };

  util.installFileErrorToString();

  console.log('Requesting filesystem.');
  chrome.fileBrowserPrivate.requestLocalFileSystem(onFileSystemFound);
}
