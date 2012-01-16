// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// If directory files changes too often, don't rescan directory more than once
// per specified interval
const SIMULTANEOUS_RESCAN_INTERVAL = 1000;

/**
 * Data model of the file manager.
 *
 * @param {DirectoryEntry} root File system root.
 * @param {boolean} singleSelection True if only one file could be seletet
 *                                  at the time.
 */
function DirectoryModel(root, singleSelection) {
  this.root_ = root;
  this.fileList_ = new cr.ui.ArrayDataModel([]);
  this.fileListSelection_ = singleSelection ?
      new cr.ui.ListSingleSelectionModel() : new cr.ui.ListSelectionModel();
  this.pendingRescanQueue_ = [];
  this.rescanRunning_ = false;
  // DirectoryEntry representing the current directory of the dialog.
  this.currentDirEntry_ = root;

  this.fileList_.prepareSort = this.prepareSort_.bind(this);
  this.autoSelectIndex_ = 0;

  this.rootsList_ = new cr.ui.ArrayDataModel([]);
  this.rootsListSelection_ = new cr.ui.ListSingleSelectionModel();
  this.rootsListSelection_.addEventListener(
      'change', this.onRootsSelectionChanged_.bind(this));
}

/**
 * The name of the directory containing externally
 * mounted removable storage volumes.
 */
DirectoryModel.REMOVABLE_DIRECTORY = 'removable';

/**
 * The name of the directory containing externally
 * mounted archive file volumes.
 */
DirectoryModel.ARCHIVE_DIRECTORY = 'archive';

/**
 * Type of a root directory.
 * @enum
 */
DirectoryModel.RootType = {
  CHROMEBOOK: 'chromebook',
  ARCHIVE: 'archive',
  REMOVABLE: 'removable'
};

/**
* The name of the downloads directory.
*/
DirectoryModel.DOWNLOADS_DIRECTORY = 'Downloads';

DirectoryModel.prototype = {
  __proto__: cr.EventTarget.prototype,

  rootPaths_: window.location.scheme == 'chrome-extension' ? null : [
    DirectoryModel.DOWNLOADS_DIRECTORY,
    DirectoryModel.REMOVABLE_DIRECTORY,
    DirectoryModel.ARCHIVE_DIRECTORY
  ],

  /**
   * Files in the current directory.
   * @type {cr.ui.ArrayDataModel}
   */
  get fileList() {
    return this.fileList_;
  },

  /**
   * Selection in the fileList.
   * @type {cr.ui.ListSelectionModel|cr.ui.ListSingleSelectionModel}
   */
  get fileListSelection() {
    return this.fileListSelection_;
  },

  /**
   * Top level Directories from user perspective.
   * @type {cr.ui.ArrayDataModel}
   */
  get rootsList() {
    return this.rootsList_;
  },

  /**
   * Selection in the rootsList.
   * @type {cr.ui.ListSingleSelectionModel}
   */
  get rootsListSelection() {
    return this.rootsListSelection_;
  },

  /**
   * Root path for the current directory (parent directory is not navigatable
   * for the user).
   * @type {string}
   */
  get rootPath() {
    return DirectoryModel.getRootPath(this.currentEntry.fullPath);
  },

  get readonly() {
    return this.isSystemDirectoy;
  },

  get isSystemDirectoy() {
    var path = this.currentEntry.fullPath;
    return path == '/' ||
           path == '/' + DirectoryModel.REMOVABLE_DIRECTORY ||
           path == '/' + DirectoryModel.ARCHIVE_DIRECTORY;
  },

  /**
   * Current directory.
   * @type {DirectoryEntry}
   */
  get currentEntry() {
    return this.currentDirEntry_;
  },

  set autoSelectIndex(value) {
    this.autoSelectIndex_ = value;
  },

  /**
   * Rescans the current directory, refreshing the list. It decreases the
   * probability that two such calls are pending simultaneously.
   *
   * This method tries to queue request if rescan is already running, and
   * processes this request later. Anyway callback would be called after
   * processing.
   *
   * If no rescan is running, then method starts rescanning immediately.
   *
   * @param {function()} opt_callback Optional function to invoke when the
   *     rescan is complete.
   */
  rescan: function(opt_callback) {
    // Clear the table first.
    this.fileList_.splice(0, this.fileList_.length);
    this.fileListSelection_.clear();

    if (this.currentDirEntry_.fullPath == '/' && this.rootPaths_) {
      this.rescanRoot_(opt_callback);
    } else {
      // Add current request to pending result list
      this.pendingRescanQueue_.push({
            onSuccess:opt_callback,
            onError:null
          });

      if (this.rescanRunning_)
        return;

      this.rescanRunning_ = true;

      // The current list of callbacks is saved and reset.  Subsequent
      // calls to rescan while we're still pending will be
      // saved and will cause an additional rescan to happen after a delay.
      var callbacks = this.pendingRescanQueue_;

      this.pendingRescanQueue_ = [];

      var self = this;
      var reader;

      function onError() {
        if (self.pendingRescanQueue_.length > 0) {
          setTimeout(self.rescan.bind(self),
              SIMULTANEOUS_RESCAN_INTERVAL);
        }

        self.rescanRunning_ = false;

        for (var i= 0; i < callbacks.length; i++) {
          if (callbacks[i].onError)
            try {
              callbacks[i].onError();
            } catch (ex) {
              console.error('Caught exception while notifying about error: ' +
                          name, ex);
            }
        }
      }

      function onReadSome(entries) {
        if (entries.length == 0) {
          metrics.recordInterval('DirectoryScan');
          if (self.currentDirEntry_.fullPath ==
              '/' + DirectoryModel.DOWNLOADS_DIRECTORY) {
            metrics.recordMediumCount("DownloadsCount", self.fileList_.length);
          }

          if (self.pendingRescanQueue_.length > 0) {
            setTimeout(self.rescan.bind(self),
                SIMULTANEOUS_RESCAN_INTERVAL);
          }

          self.rescanRunning_ = false;
          for (var i= 0; i < callbacks.length; i++) {
            if (callbacks[i].onSuccess)
              try {
                callbacks[i].onSuccess();
              } catch (ex) {
                console.error('Caught exception while notifying about error: ' +
                            name, ex);
              }
          }

          return;
        }

        // Splice takes the to-be-spliced-in array as individual parameters,
        // rather than as an array, so we need to perform some acrobatics...
        var spliceArgs = [].slice.call(entries);

        // Hide files that start with a dot ('.').
        // TODO(rginda): User should be able to override this. Support for other
        // commonly hidden patterns might be nice too.
        if (self.filterFiles_) {
          spliceArgs = spliceArgs.filter(function(e) {
              return e.name.substr(0, 1) != '.';
            });
        }

        self.prefetchCacheForSorting_(spliceArgs, function() {
          spliceArgs.unshift(0, 0);  // index, deleteCount
          self.fileList_.splice.apply(self.fileList_, spliceArgs);

          // Keep reading until entries.length is 0.
          reader.readEntries(onReadSome, onError);
        });
      };

      metrics.startInterval('DirectoryScan');

      // If not the root directory, just read the contents.
      reader = this.currentDirEntry_.createReader();
      reader.readEntries(onReadSome, onError);
      return;
    }
  },

  prefetchCacheForSorting_: function(entries, callback) {
    var field = this.fileList_.sortStatus.field;
    if (field) {
      this.prepareSortEntries_(entries, field, callback);
    } else {
      callback();
      return;
    }
  },

  // TODO(serya): With left panel we probably don't need it.
  rescanRoot_: function(opt_callback) {
    var dm = this.fileList_;
    var spliceArgs = [0, 0];
    var onDone = function() {
      dm.splice.apply(dm, spliceArgs);
      if (opt_callback)
        opt_callback();
    };

    function onPathError(path, err) {
      console.error('Error locating root path: ' + path + ': ' + err);
    }

    function onEntryFound(entry) {
      if (entry)
        spliceArgs.push(entry);
      else
        onDone();
    }

    util.getDirectories(this.root_, {create: false}, this.rootPaths_,
                        onEntryFound, onPathError);
  },

  /**
   * Rescans directory later.
   * This method should be used if we just want rescan but not actually now.
   * This helps us not to flood queue with rescan requests.
   */
  rescanLater: function() {
    // It might be massive change, so let's note somehow, that we need
    // rescanning and then wait some time

    if (this.pendingRescanQueue_.length == 0) {
      // If rescan isn't going to run without
      // our interruption, then say that we need to run it
      if (!this.rescanRunning_) {
        setTimeout(this.rescan.bind(this),
            SIMULTANEOUS_RESCAN_INTERVAL);
      }
    }
  },

  /**
   * Delete the list of files and directories from filesystem and
   * update the file list.
   * @param {Array.<Entry>} entries Entries to delete.
   * @param {Function} opt_callback Called when finished.
   */
  deleteEntries: function(entries, opt_callback) {
    var downcount = entries.length + 1;

    var onComplete = function() {
      if (--downcount == 0) {
        this.rescan();
        if (opt_callback)
          opt_callback();
      }
    }.bind(this);

    for (var i = 0; i < entries.length; i++) {
      var entry = entries[i];
      if (entry.isFile) {
        entry.remove(
            onComplete,
            util.flog('Error deleting file: ' + entry.fullPath, onComplete));
      } else {
        entry.removeRecursively(
            onComplete,
            util.flog('Error deleting folder: ' + entry.fullPath, onComplete));
      }
    }
    onComplete();
  },

  /**
   * Rename the entry in the filesystem and update the file list.
   * @param {Entry} entry Entry to rename.
   * @param {string} newName
   * @param {Function} errorCallback Called on error.
   * @param {Function} opt_successCallback Called on success.
   */
  renameEntry: function(entry, newName, errorCallback, opt_successCallback) {
    function onRescan() {
      this.selectEntry(newName);
      if (opt_successCallback)
        opt_successCallback();
    }
    var onSuccess = this.rescan.bind(this, onRescan.bind(this));
    entry.moveTo(this.currentEntry, newName,
                 onSuccess, errorCallback);
  },

  /**
   * Checks if current directory contains a file or directory with this name.
   * @param {string} newName Name to cjeck.
   * @param {function(boolean, boolean?)} callback Called when the result's
   *     available. First parameter is true if the entry exists and second
   *     is true if it's a file.
   */
  doesExist: function(newName, callback) {
    util.resolvePath(this.currentEntry, newName,
        function(entry) {
          callback(true, entry.isFile);
        },
        callback.bind(window, false));
  },

  /**
   * Creates directory and updates the file list.
   */
  createDirectory: function(name, successCallback, errorCallback) {
    var onSuccess = this.rescan.bind(this, function() {
      this.selectEntry(name);
      successCallback();
    }.bind(this));
    this.currentEntry.getDirectory(name, {create: true, exclusive: true},
                                   onSuccess, errorCallback);
  },

  /**
   * Canges directory. Causes 'directory-change' event.
   *
   * @param {string} path New current directory path.
   */
  changeDirectory: function(path) {
    var onDirectoryResolved = function(dirEntry) {
      var autoSelect = this.selectIndex.bind(this, this.autoSelectIndex_);
      this.changeDirectoryEntry_(dirEntry, autoSelect, false);
    }.bind(this);

    if (path == '/')
      return onDirectoryResolved(this.root_);

    this.root_.getDirectory(
        path, {create: false},
        onDirectoryResolved,
        function(error) {
          // TODO(serya): We should show an alert.
          console.error('Error changing directory to: ' + path + ', ' + error);
        });
  },

  /**
   * Change the current directory to the directory represented by a
   * DirectoryEntry.
   *
   * Dispatches the 'directory-changed' event when the directory is successfully
   * changed.
   *
   * @param {DirectoryEntry} dirEntry The absolute path to the new directory.
   * @param {function} action Action executed when the directory loaded.
   *                              By default selects the first item
   *                              (unless it's a save dialog).
   * @param {boolean} initial True if it comes from setupPath and
   *                          false if caused by an user action.
   */
  changeDirectoryEntry_: function(dirEntry, action, initial) {
    var current = this.currentEntry;
    this.currentDirEntry_ = dirEntry;
    function onRescanComplete() {
      action();
      // For tests that open the dialog to empty directories, everything
      // is loaded at this point.
      chrome.test.sendMessage('directory-change-complete');
    }
    this.updateRootsListSelection_();
    this.rescan(onRescanComplete);

    var e = new cr.Event('directory-changed');
    e.previousDirEntry = this.currentEntry;
    e.newDirEntry = dirEntry;
    e.initial = initial;
    this.dispatchEvent(e);
  },

  setupPath: function(path, opt_pathResolveCallback) {
    // Split the dirname from the basename.
    var ary = path.match(/^(?:(.*)\/)?([^\/]*)$/);
    var autoSelect = this.selectIndex.bind(this, this.autoSelectIndex_);
    if (!ary) {
      console.warn('Unable to split default path: ' + path);
      this.changeDirectoryEntry_(this.root_, autoSelect, true);
      return;
    }

    var baseName = ary[1];
    var leafName = ary[2];

    function callBack() {
      if (opt_pathResolveCallback)
        opt_pathResolveCallback(baseName, leafName);
    }

    function onLeafFound(baseDirEntry, leafEntry) {
      if (leafEntry.isDirectory) {
        baseName = path;
        leafName = '';
        callBack();
        this.changeDirectoryEntry_(leafEntry, autoSelect, true);
        return;
      }

      callBack();
      // Leaf is an existing file, cd to its parent directory and select it.
      this.changeDirectoryEntry_(baseDirEntry,
                                 this.selectEntry.bind(this, leafEntry.name),
                                 true);
    }

    function onLeafError(baseDirEntry, err) {
      callBack();
      if (err = FileError.NOT_FOUND_ERR) {
        // Leaf does not exist, it's just a suggested file name.
        this.changeDirectoryEntry_(baseDirEntry, autoSelect);
      } else {
        console.log('Unexpected error resolving default leaf: ' + err);
        this.changeDirectoryEntry_(this.root_, autoSelect, true);
      }
    }

    var onBaseError = function(err) {
      callBack();
      console.log('Unexpected error resolving default base "' +
                  baseName + '": ' + err);
      this.changeDirectory('/', undefined, true);
    }.bind(this);

    var onBaseFound = function(baseDirEntry) {
      if (!leafName) {
        // Default path is just a directory, cd to it and we're done.
        this.changeDirectoryEntry_(baseDirEntry, autoSelect, true);
        return;
      }

      util.resolvePath(this.root_, path,
                       onLeafFound.bind(this, baseDirEntry),
                       onLeafError.bind(this, baseDirEntry));
    }.bind(this);

    var root = this.root_;
    if (baseName) {
      root.getDirectory(
          baseName, {create: false}, onBaseFound, onBaseError);
    } else {
      this.getDefaultDirectory_(function(defaultDir) {
        baseName = defaultDir;
        root.getDirectory(
            baseName, {create: false}, onBaseFound, onBaseError);
      });
    }
  },

  setupDefaultPath: function() {
    this.getDefaultDirectory_(this.setupPath.bind(this));
  },

  getDefaultDirectory_: function(callback) {
    function onGetDirectoryComplete(entries, error) {
      if (entries.length > 0)
        callback(entries[0].fullPath);
      else
        callback('/' + DirectoryModel.DOWNLOADS_DIRECTORY);
    }

    // No preset given, find a good place to start.
    // Check for removable devices, if there are none, go to Downloads.
    util.readDirectory(this.root_, DirectoryModel.REMOVABLE_DIRECTORY,
                       onGetDirectoryComplete);
  },

  selectEntry: function(name) {
    var dm = this.fileList_;
    for (var i = 0; i < dm.length; i++) {
      if (dm.item(i).name == name) {
        this.selectIndex(i);
        return;
      }
    }
  },

  selectIndex: function(index) {
    // this.focusCurrentList_();
    if (index >= this.fileList_.length)
      return;

    // If a list bound with the model it will do scrollIndexIntoView(index).
    this.fileListSelection_.selectedIndex = index;
  },

  /**
   * Cache necessary data before a sort happens.
   *
   * This is called by the table code before a sort happens, so that we can
   * go fetch data for the sort field that we may not have yet.
   */
  prepareSort_: function(field, callback) {
    this.prepareSortEntries_(this.fileList_.slice(), field, callback);
  },

  prepareSortEntries_: function(entries, field, callback) {
    var cacheFunction;

    if (field == 'name' || field == 'cachedMtime_') {
      // Mtime is the tie-breaker for a name sort, so we need to resolve
      // it for both mtime and name sorts.
      cacheFunction = this.cacheEntryDate;
    } else if (field == 'cachedSize_') {
      cacheFunction = this.cacheEntrySize;
    } else if (field == 'type') {
      cacheFunction = this.cacheEntryFileType;
    } else if (field == 'cachedIconType_') {
      cacheFunction = this.cacheEntryIconType;
    } else {
      setTimeout(callback, 0);
      return;
    }

    // Start one fake wait to prevent calling the callback twice.
    var waitCount = 1;
    for (var i = 0; i < entries.length ; i++) {
      var entry = entries[i];
      if (!(field in entry)) {
        waitCount++;
        cacheFunction(entry, onCacheDone)
      }
    }
    onCacheDone();  // Finish the fake callback.

    function onCacheDone() {
      waitCount--;
      // If all caching functions finished synchronously or entries.length = 0
      // call the callback synchronously.
      if (waitCount == 0)
        setTimeout(callback, 0);
    }
  },

  /**
   * Get root entries asynchronously. Invokes callback
   * when have finished.
   */
  resolveRoots_: function(callback) {
    var groups = {
      downloads: null,
      archives: null,
      removables: null
    };

    metrics.startInterval('Load.Roots');
    function done() {
      for (var i in groups)
        if (!groups[i])
          return;

      callback(groups.downloads.
               concat(groups.archives).
               concat(groups.removables));
      metrics.recordInterval('Load.Roots');
    }

    function append(index, values, opt_error) {
      groups[index] = values;
      done();
    }

    function onDownloads(entry) {
      groups.downloads = [entry];
      done();
    }

    function onDownloadsError(error) {
      groups.downloads = [];
      done();
    }

    var root = this.root_;
    root.getDirectory(DirectoryModel.DOWNLOADS_DIRECTORY, { create: false },
                      onDownloads, onDownloadsError);
    util.readDirectory(root, DirectoryModel.ARCHIVE_DIRECTORY,
                       append.bind(this, 'archives'));
    util.readDirectory(root, DirectoryModel.REMOVABLE_DIRECTORY,
                       append.bind(this, 'removables'));
  },

  updateRoots: function(opt_changeDirectoryTo) {
    var self = this;
    this.resolveRoots_(function(rootEntries) {
      var dm = self.rootsList_;
      var args = [0, dm.length].concat(rootEntries);
      dm.splice.apply(dm, args);

      self.updateRootsListSelection_();

      if (opt_changeDirectoryTo)
        self.changeDirectory(opt_changeDirectoryTo);
    });
  },

  onRootsSelectionChanged_: function(event) {
    var root = this.rootsList.item(this.rootsListSelection.selectedIndex);
    var current = this.currentEntry.fullPath;
    if (root && this.rootPath != root.fullPath)
      this.changeDirectory(root.fullPath);
  },

  updateRootsListSelection_: function() {
    var roots = this.rootsList_;
    var rootPath = this.rootPath;
    for (var index = 0; index < roots.length; index++) {
      if (roots.item(index).fullPath == rootPath) {
        this.rootsListSelection.selectedIndex = index;
        return;
      }
    }
    this.rootsListSelection.selectedIndex = -1;
  }
};

DirectoryModel.getRootPath = function(path) {
  var type = DirectoryModel.getRootType(path);

  if (type == DirectoryModel.RootType.CHROMEBOOK)
    return '/' + DirectoryModel.DOWNLOADS_DIRECTORY;

  function subdir(dir) {
    var end = path.indexOf('/', dir.length + 2);
    return end == -1 ? path : path.substr(0, end);
  }

  if (type == DirectoryModel.RootType.ARCHIVE)
    return subdir(DirectoryModel.ARCHIVE_DIRECTORY);
  if (type == DirectoryModel.REMOVABLE_DIRECTORY)
    return subdir(DirectoryModel.REMOVABLE_DIRECTORY);
  return '/';
};

DirectoryModel.getRootType = function(path) {
  function isTop(dir) {
    return path.substr(1, dir.length) == dir;
  }

  if (isTop(DirectoryModel.DOWNLOADS_DIRECTORY))
    return DirectoryModel.RootType.CHROMEBOOK;
  else if (isTop(DirectoryModel.ARCHIVE_DIRECTORY))
    return DirectoryModel.RootType.ARCHIVE;
  else if(isTop(DirectoryModel.REMOVABLE_DIRECTORY))
    return DirectoryModel.RootType.REMOVABLE;
  return '';
};

