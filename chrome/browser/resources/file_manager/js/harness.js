// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var harness = {
  /**
   * Kick off the test harness.
   *
   * Called by harness.html after the dom has been parsed.
   */
  init: function() {
    console.log('Initializing harness...');

    util.installFileErrorToString();

    var self = this;

    function onFilesystem(filesystem) {
      console.log('Filesystem found.');
      self.filesystem = filesystem;
      util.getOrCreateDirectory(filesystem.root, '/Downloads', function () {});
      util.getOrCreateDirectory(filesystem.root, '/removable', function () {});
      util.getOrCreateDirectory(filesystem.root, '/removable/disk1',
          function () {});
      util.getOrCreateDirectory(filesystem.root, '/removable/disk2',
          function () {});
    };

    window.webkitRequestFileSystem(window.PERSISTENT, 16 * 1024 * 1024,
                                   onFilesystem,
                                   util.flog('Error initializing filesystem'));

    var paramstr = decodeURIComponent(document.location.search.substr(1));
    this.params = paramstr ? JSON.parse(paramstr) : {};

    var input = document.getElementById('default-path');
    input.value = this.params.defaultPath || '';
    input.addEventListener('keyup', this.onInputKeyUp.bind(this));

    var iframe = document.getElementById('dialog');
    iframe.setAttribute('src', 'main.html' + document.location.search);
  },

  onInputKeyUp: function(event) {
    if (event.keyCode != 13)
      return;

    this.changePath();
  },

  changePath: function() {
    var input = document.getElementById('default-path');
    this.changeParam('defaultPath', input.value);
  },

  changeParam: function(name, value) {
    this.params[name] = value;
    document.location.href = '?' + JSON.stringify(this.params);
  },

  /**
   * 'Reset Fileystem' button click handler.
   */
  onClearClick: function() {
    util.forEachDirEntry(this.filesystem.root, function(dirEntry) {
      if (!dirEntry)
        return console.log('Filesystem reset.');

      console.log('Remove: ' + dirEntry.name);

      if (dirEntry.isDirectory) {
        dirEntry.removeRecursively();
      } else {
        dirEntry.remove();
      }
    });
  },

  /**
   * Change handler for the 'input type=file' element.
   */
  onFilesChange: function(event) {
    this.importFiles([].slice.call(event.target.files));
  },

  /**
   * The fileManager object under test.
   *
   * This is a getter rather than a normal property because the fileManager
   * is initialized asynchronously, and we won't be sure when it'll be
   * done.  Since harness.fileManager is intended to be used for debugging
   * from the JS console, we don't really need to be sure it's ready at any
   * particular time.
   */
  get fileManager() {
    return document.getElementById('dialog').contentWindow.fileManager;
  },
  get pyautoAPI() {
    return document.getElementById('dialog').contentWindow.pyautoAPI;
  },

  /**
   * Import a list of File objects into harness.filesystem.
   */
  importFiles: function(files) {
    var currentSrc = null;
    var currentDest = null;
    var importCount = 0;

    var self = this;

    function onWriterCreated(writer) {
      writer.onerror = util.flog('Error writing: ' + currentDest.fullPath);
      writer.onwriteend = function() {
        console.log('Wrote: ' + currentDest.fullPath);
        //console.log(writer);
        //console.log(currentDest);
        ++importCount;
        processNextFile();
      };

      writer.write(currentSrc);
    }

    function onFileFound(fileEntry) {
      currentDest = fileEntry;
      currentDest.createWriter(onWriterCreated,
                               util.flog('Error creating writer for: ' +
                                         currentDest.fullPath));
    }

    function processNextFile() {
      if (files.length == 0) {
        console.log('Import complete: ' + importCount + ' file(s)');
        return;
      }

      currentSrc = files.shift();
      var destPath = harness.fileManager.currentDirEntry_.fullPath + '/' +
          currentSrc.name.replace(/\^\^/g, '/');
      util.getOrCreateFile(self.filesystem.root, destPath, onFileFound,
                           util.flog('Error finding path: ' + destPath));
    }

    console.log('Start import: ' + files.length + ' file(s)');
    processNextFile();
  },
};
