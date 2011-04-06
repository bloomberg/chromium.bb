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
  var params;

  function onFileSystemFound(filesystem) {
    FileManager.initStrings(function () {
      fileManager = new FileManager(document.body, filesystem, params);
    });
  };

  if (document.location.search) {
    var json = decodeURIComponent(document.location.search.substr(1));
    console.log('params: ' + json);
    params = JSON.parse(json);
  }

  util.installFileErrorToString();

  chrome.fileBrowserPrivate.requestLocalFileSystem(onFileSystemFound);
}
