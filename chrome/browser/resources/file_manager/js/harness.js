// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

    function onFilesystem(filesystem) {
      console.log('Filesystem found.');
      harness.filesystem = filesystem;
      chrome.fileBrowserPrivate.getMountPoints(function(mountPoints) {
        var roots = ['/Downloads', '/removable', '/archives'];
        for (var i = 0; i != mountPoints.length; i++) {
          roots.push(mountPoints[i].mountPath);
        }
        createRoots(roots);
      });
    }

    function createRoots(roots) {
      if (roots.length == 0) {
        loadUI();
        return;
      }
      var root = roots.shift();
      util.getOrCreateDirectory(harness.filesystem.root, root,
          function(dir) {
            console.log('Created/found', dir.fullPath);
            createRoots(roots);
          },
          function(err) {
            console.log('Error creating ' + root + ':' + err.toString());
            createRoots(roots);
          });
    }

    function loadUI() {
      var iframe = document.getElementById('dialog');
      iframe.setAttribute('src', 'main.html' + document.location.search);
    }

    window.webkitStorageInfo.requestQuota(
        window.PERSISTENT,
        1024*1024*1024, // 1 Gig should be enough for everybody:)
        function(grantedBytes) {
          window.webkitRequestFileSystem(
              window.PERSISTENT,
              grantedBytes,
              onFilesystem,
              util.flog('Error initializing filesystem'));
        },
        util.flog('Error requesting filesystem quota'));

    var paramstr = decodeURIComponent(document.location.search.substr(1));
    this.params = paramstr ? JSON.parse(paramstr) : {};

    var input = document.getElementById('default-path');
    input.value = this.params.defaultPath || '';
    input.addEventListener('keyup', this.onInputKeyUp.bind(this));
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
      if (!dirEntry) {
        console.log('Filesystem reset.');
        harness.init();
        return;
      }
      util.removeFileOrDirectory(
          dirEntry,
          util.flog('Removed ' + dirEntry.name),
          util.flog('Error deleting ' + dirEntry.name));
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
  get chrome() {
    return document.getElementById('dialog').contentWindow.chrome;
  },

  /**
   * Import a list of File objects into harness.filesystem.
   */
  importFiles: function(files) {
    var currentSrc = null;
    var currentDest = null;
    var importCount = 0;

    function onWriterCreated(writer) {
      writer.onerror = util.flog('Error writing: ' + currentDest.fullPath);
      writer.onwriteend = function() {
        console.log('Wrote: ' + currentDest.fullPath);
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
        harness.chrome.fileBrowserPrivate.onFileChanged.notify({
          fileUrl: harness.fileManager.getCurrentDirectoryURL()
        });
        return;
      }

      currentSrc = files.shift();
      var destPath = harness.fileManager.getCurrentDirectory() + '/' +
          currentSrc.name.replace(/\^\^/g, '/');
      util.getOrCreateFile(harness.filesystem.root, destPath, onFileFound,
                           util.flog('Error finding path: ' + destPath));
    }

    console.log('Start import: ' + files.length + ' file(s)');
    processNextFile();
  }
};
