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
  function onFileSystemFound(filesystem) {
    FileManager.initStrings(function () {
      fileManager = new FileManager(document.body, filesystem);
    });
  };

  util.installFileErrorToString();

  chrome.fileBrowserPrivate.requestLocalFileSystem(onFileSystemFound);
}
