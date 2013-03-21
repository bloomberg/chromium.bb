// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var harness = {
  /**
   * Kick off the test harness.
   *
   * Called by harness.html after the dom has been parsed.
   */
  init: function() {
    console.log('Initializing harness...');
    harness.initPathControls();
    harness.initListeners();
    harness.initFileSystem(function() {
      harness.iframe.src = 'main.html' + document.location.search;
    });
  },

  /**
   * Init the test harness when it is emmbedded into the File Manager page.
   * @param {function} callback Callback.
   */
  initEmbedded: function(callback) {
    var harnessControls = util.createChild(document.body);
    harnessControls.style.cssText =
        'position: absolute; left: 4px; bottom: 4px; width: 94px;' +
        'z-index: 100; opacity: 0.3';

    var createControl = function(tag) {
      return util.createChild(harnessControls, '', tag);
    };

    var fileChooser = createControl('input');
    fileChooser.type = 'file';
    fileChooser.multiple = 'true';
    fileChooser.style.width = '100%';

    createControl('br');

    var resetButton = createControl('button');
    resetButton.id = 'reset';
    resetButton.textContent = 'Reset';
    resetButton.style.width = '100%';

    createControl('br');

    var populateButton = createControl('button');
    populateButton.id = 'populate';
    populateButton.textContent = 'Populate';
    populateButton.style.width = '100%';

    harness.initListeners();
    harness.initFileSystem(callback);
  },

  initFileSystem: function(opt_callback) {
    util.installFileErrorToString();

    var onFilesystem = function(filesystem) {
      console.log('Filesystem found.');
      harness.filesystem = filesystem;
      chrome.fileBrowserPrivate.getMountPoints(function(mountPoints) {
        var roots = ['/Downloads', '/removable', '/archives'];
        for (var i = 0; i != mountPoints.length; i++) {
          roots.push(mountPoints[i].mountPath);
        }
        createRoots(roots);
      });
    };

    var createRoots = function(roots) {
      if (roots.length == 0) {
        if (harness.fileManager)
          harness.fileManager.directoryModel_.changeDirectory('/Downloads');
        if (opt_callback) opt_callback();
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
    };

    window.webkitStorageInfo.requestQuota(
        chrome.fileBrowserPrivate.FS_TYPE,
        1024 * 1024 * 1024, // 1 Gig should be enough for everybody:)
        function(grantedBytes) {
          window.webkitRequestFileSystem(
              chrome.fileBrowserPrivate.FS_TYPE,
              grantedBytes,
              onFilesystem,
              util.flog('Error initializing filesystem'));
        },
        util.flog('Error requesting filesystem quota'));
  },

  initPathControls: function() {
    var paramstr = decodeURIComponent(document.location.search.substr(1));
    harness.params = paramstr ? JSON.parse(paramstr) : {};

    var input = document.querySelector('#default-path');
    input.value = harness.params.defaultPath || '';
    input.addEventListener('keyup', harness.onInputKeyUp);
  },

  initListeners: function() {
    document.querySelector('input[type="file"]').
        addEventListener('change', harness.onFilesChange);
    document.querySelector('button#reset').
        addEventListener('click', harness.onClearClick);
    document.querySelector('button#populate').
        addEventListener('click', harness.onPopulateClick);
  },

  onInputKeyUp: function(event) {
    if (event.keyCode != 13)
      return;

    harness.changePath();
  },

  changePath: function() {
    var input = document.querySelector('#default-path');
    harness.changeParam('defaultPath', input.value);
  },

  changeParam: function(name, value) {
    harness.params[name] = value;
    document.location.href = '?' + JSON.stringify(harness.params);
  },

  /**
   * 'Reset Filesystem' button click handler.
   */
  onClearClick: function() {
    harness.resetFilesystem(harness.filesystem, harness.initFileSystem);
  },

  resetFilesystem: function(filesystem, opt_callback) {
    util.forEachDirEntry(filesystem.root, function(dirEntry) {
      if (!dirEntry) {
        console.log('Filesystem reset.');
        if (opt_callback) opt_callback();
        return;
      }
      util.removeFileOrDirectory(
          dirEntry,
          util.flog('Removed ' + dirEntry.name),
          util.flog('Error deleting ' + dirEntry.name));
    });
  },

  /**
   * 'Auto-populate' button click handler.
   */
  onPopulateClick: function() {
    harness.importWebDirectory(harness.filesystem,
        'Downloads', 'harness_files', function() {}, harness.refreshDirectory);
  },

  /**
   * Change handler for the 'input type=file' element.
   */
  onFilesChange: function(event) {
    harness.importFiles(harness.filesystem,
        harness.fileManager.getCurrentDirectory(),
        [].slice.call(event.target.files),
        harness.refreshDirectory.bind(harness));
  },

  /**
   * Force the file manager to refresh the current directory.
   */
  refreshDirectory: function() {
    harness.chrome.fileBrowserPrivate.onDirectoryChanged.notify({
      directoryUrl: harness.fileManager.getCurrentDirectoryURL()
    });
  },

  get iframe() { return document.querySelector('iframe#dialog') },

  get contentWindow() {
    if (harness.iframe != null)
      return harness.iframe.contentWindow;
    return window;
  },

  get fileManager() { return harness.contentWindow.fileManager },

  get pyautoAPI() { return harness.contentWindow.pyautoAPI },

  get chrome() { return harness.contentWindow.chrome },

  /**
   * Copy a blob into the filesystem.
   *
   * @param {DOMFileSystem} filesystem Destination file system.
   * @param {string} dstPath Destination file path.
   * @param {Blob} srcBlob Source blob.
   * @param {function} callback Completion callback.
   */
  copyBlob: function(filesystem, dstPath, srcBlob, callback) {
    var onWriterCreated = function(entry, writer) {
      writer.onerror =
          util.flog('Error writing: ' + entry.fullPath, callback);
      writer.onwriteend = function() {
        console.log('Wrote ' + srcBlob.size + ' bytes to ' + entry.fullPath);
        callback();
      };

      writer.write(srcBlob);
    };

    var onFileFound = function(fileEntry) {
      fileEntry.createWriter(onWriterCreated.bind(null, fileEntry), util.flog(
          'Error creating writer for: ' + fileEntry.fullPath, callback));
    };

    util.getOrCreateFile(filesystem.root, dstPath, onFileFound,
        util.flog('Error finding path: ' + dstPath, callback));
  },

  /**
   * Import a list of File objects into harness.filesystem.
   *
   * @param {DOMFileSystem} filesystem File system.
   * @param {string} dstDir Destination path.
   * @param {Array.<File>} files Array of files.
   * @param {function} callback Completion callback.
   */
  importFiles: function(filesystem, dstDir, files, callback) {
    var processNextFile = function() {
      if (files.length == 0) {
        console.log('Import complete');
        callback();
        return;
      }

      var file = files.shift();
      harness.copyBlob(
          filesystem, dstDir + '/' + file.name, file, processNextFile);
    };

    console.log('Start import: ' + files.length + ' file(s)');
    processNextFile();
  },

  /**
   * Copy all files recursively linked from a web page.
   *
   * Assumes the directory listing format produced by Python's SimpleHTTPServer:
   * Child names are in |href| attributes, subdirectory names end with '/'.
   *
   * @param {DOMFileSystem} filesystem File system.
   * @param {string} dstDir Destination path.
   * @param {string} srcDirUrl Directory page url.
   * @param {function(string, Blob)} onProgress Progress callback.
   * @param {function} onComplete Completion callback.
   */
  importWebDirectory: function(
      filesystem, dstDirPath, srcDirUrl, onProgress, onComplete) {
    var childNames = [];

    var readFromUrl = function(url, type, callback) {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', url, true);
      xhr.responseType = type;
      xhr.onload = function() {
        if (xhr.status == 200)
          callback(
              xhr.responseType == 'text' ? xhr.responseText : xhr.response);
        else
          callback(null);
      };
      xhr.send();
    };

    var processNextChild = function() {
      if (childNames.length == 0) {
        onComplete();
        return;
      }

      var name = childNames.shift();
      var isDirectory = name.match(/\/$/);  // Ends with '/'.
      if (isDirectory) name = name.substring(0, name.length - 1);

      var srcChildUrl = srcDirUrl + '/' + name;
      var dstChildPath = dstDirPath + '/' + name;
      if (isDirectory) {
        harness.importWebDirectory(filesystem,
            dstChildPath, srcChildUrl, onProgress, processNextChild);
      } else {
        readFromUrl(srcChildUrl, 'blob', function(blob) {
          onProgress(blob);
          harness.copyBlob(filesystem, dstChildPath, blob, processNextChild);
        });
      }
    };

    readFromUrl(srcDirUrl, 'text', function(text) {
      if (!text) {
        console.log('Cannot read ' + srcDirUrl);
        onComplete();
        return;
      }
      var links = text.match(/href=".*"/g);  // Extract all links from the page.
      if (links) {
        for (var i = 0; i != links.length; i++)
          childNames.push(links[i].match(/href="(.*)"/)[1]);
      }
      console.log(
          'Importing ' + childNames.length + ' entries from ' + srcDirUrl);
      processNextChild();
    });
  }
};

document.addEventListener('DOMContentLoaded', harness.init.bind(harness));
