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
    };

    window.requestFileSystem(window.PERSISTENT, 16 * 1024 * 1024,
                             onFilesystem,
                             util.flog('Error initializing filesystem'));
  },

  /**
   * Utility function to invoke callback once for each entry in dirEntry.
   *
   * @param {DirectoryEntry} dirEntry The directory entry to enumerate.
   * @param {function(Entry)} callback The function to invoke for each entry in
   *   dirEntry.
   */
  forEachDirEntry: function(dirEntry, callback) {
    var reader;

    function onReadSome(results) {
      if (results.length == 0)
        return callback(null);

      for (var i = 0; i < results.length; ++i)
        callback(results[i]);

      reader.readEntries(onReadSome);
    };

    reader = dirEntry.createReader();
    reader.readEntries(onReadSome);
  },

  /**
   * 'Reset Fileystem' button click handler.
   */
  onClearClick: function() {
    this.forEachDirEntry(this.filesystem.root, function(dirEntry) {
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
      var destPath = currentSrc.name.replace(/\^\^/g, '/');
      self.getOrCreateFile(destPath, onFileFound,
                              util.flog('Error finding path: ' + destPath));
    }

    console.log('Start import: ' + files.length + ' file(s)');
    processNextFile();
  },

  /**
   * Locate the file referred to by path, creating directories or the file
   * itself if necessary.
   */
  getOrCreateFile: function(path, successCallback, errorCallback) {
    var dirname = null;
    var basename = null;

    function onDirFound(dirEntry) {
      dirEntry.getFile(basename, { create: true },
                       successCallback, errorCallback);
    }

    var i = path.lastIndexOf('/');
    if (i > -1) {
      dirname = path.substr(0, i);
      basename = path.substr(i + 1);
    } else {
      basename = path;
    }

    if (!dirname)
      return onDirFound(this.filesystem.root);

    this.getOrCreatePath(dirname, onDirFound, errorCallback);
  },

  /**
   * Locate the directory referred to by path, creating directories along the
   * way.
   */
  getOrCreatePath: function(path, successCallback, errorCallback) {
    var names = path.split('/');

    function getOrCreateNextName(dir) {
      if (!names.length)
        return successCallback(dir);

      var name;
      do {
        name = names.shift();
      } while (!name || name == '.');

      dir.getDirectory(name, { create: true }, getOrCreateNextName,
                       errorCallback);
    }

    getOrCreateNextName(this.filesystem.root);
  }
};
