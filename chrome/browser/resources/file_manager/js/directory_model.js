// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// If directory files changes too often, don't rescan directory more than once
// per specified interval
var SIMULTANEOUS_RESCAN_INTERVAL = 1000;
// Used for operations that require almost instant rescan.
var SHORT_RESCAN_INTERVAL = 100;

/**
 * Data model of the file manager.
 *
 * @constructor
 * @param {DirectoryEntry} root File system root.
 * @param {boolean} singleSelection True if only one file could be selected
 *                                  at the time.
 * @param {boolean} showGData Defines whether GData root should be should
 *   (regardless of its mounts status).
 * @param {MetadataCache} metadataCache The metadata cache service.
 */
function DirectoryModel(root, singleSelection, showGData, metadataCache) {
  this.root_ = root;
  this.metadataCache_ = metadataCache;
  this.fileList_ = new cr.ui.ArrayDataModel([]);
  this.fileListSelection_ = singleSelection ?
      new cr.ui.ListSingleSelectionModel() : new cr.ui.ListSelectionModel();

  this.showGData_ = showGData;

  this.runningScan_ = null;
  this.pendingScan_ = null;
  this.rescanTimeout_ = undefined;
  this.scanFailures_ = 0;

  // DirectoryEntry representing the current directory of the dialog.
  this.currentDirEntry_ = root;

  this.fileList_.prepareSort = this.prepareSort_.bind(this);

  this.rootsList_ = new cr.ui.ArrayDataModel([]);
  this.rootsListSelection_ = new cr.ui.ListSingleSelectionModel();

  /**
   * A map root.fullPath -> currentDirectory.fullPath.
   * @private
   * @type {Object.<string, string>}
   */
  this.currentDirByRoot_ = {};

  // The map 'name' -> callback. Callbacks are function(entry) -> boolean.
  this.filters_ = {};
  this.setFilterHidden(true);

  /**
   * @private
   * @type {Object.<string, boolean>}
   */
  this.volumeReadOnlyStatus_ = {};
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
  DOWNLOADS: 'downloads',
  ARCHIVE: 'archive',
  REMOVABLE: 'removable',
  GDATA: 'gdata'
};

/**
* The name of the downloads directory.
*/
DirectoryModel.DOWNLOADS_DIRECTORY = 'Downloads';

/**
* The name of the gdata provider directory.
*/
DirectoryModel.GDATA_DIRECTORY = 'drive';

/**
 * DirectoryModel extends cr.EventTarget.
 */
DirectoryModel.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * @return {cr.ui.ArrayDataModel} Files in the current directory.
 */
DirectoryModel.prototype.getFileList = function() {
  return this.fileList_;
};

/**
 * @return {MetadataCache} Metadata cache.
 */
DirectoryModel.prototype.getMetadataCache = function() {
  return this.metadataCache_;
};

/**
 * Sort the file list.
 * @param {string} sortField Sort field.
 * @param {string} sortDirection "asc" or "desc".
 */
DirectoryModel.prototype.sortFileList = function(sortField, sortDirection) {
  this.fileList_.sort(sortField, sortDirection);
};

/**
 * @return {cr.ui.ListSelectionModel|cr.ui.ListSingleSelectionModel} Selection
 * in the fileList.
 */
DirectoryModel.prototype.getFileListSelection = function() {
  return this.fileListSelection_;
};

/**
 * @return {DirectoryModel.RootType} Root type of current root.
 */
DirectoryModel.prototype.getRootType = function() {
  return DirectoryModel.getRootType(this.currentDirEntry_.fullPath);
};

/**
 * @return {string} Root name.
 */
DirectoryModel.prototype.getRootName = function() {
  return DirectoryModel.getRootName(this.currentDirEntry_.fullPath);
};

/**
 * @return {boolean} on True if offline.
 */
DirectoryModel.prototype.isOffline = function() {
  return this.offline_;
};

/**
 * @param {boolean} offline True if offline.
 */
DirectoryModel.prototype.setOffline = function(offline) {
  this.offline_ = offline;
};

/**
 * @return {boolean} True if current directory is read only.
 */
DirectoryModel.prototype.isReadOnly = function() {
  return this.isPathReadOnly(this.getCurrentRootPath());
};

/**
 * @param {string} path Path to check.
 * @return {boolean} True if the |path| is read only.
 */
DirectoryModel.prototype.isPathReadOnly = function(path) {
  switch (DirectoryModel.getRootType(path)) {
    case DirectoryModel.RootType.REMOVABLE:
      return !!this.volumeReadOnlyStatus_[DirectoryModel.getRootPath(path)];
    case DirectoryModel.RootType.ARCHIVE:
      return true;
    case DirectoryModel.RootType.DOWNLOADS:
      return false;
    case DirectoryModel.RootType.GDATA:
      return this.isOffline();
    default:
      return true;
  }
};

/**
 * @return {boolean} If current directory is system.
 */
DirectoryModel.prototype.isSystemDirectory = function() {
  var path = this.currentDirEntry_.fullPath;
  return path == '/' ||
         path == '/' + DirectoryModel.REMOVABLE_DIRECTORY ||
         path == '/' + DirectoryModel.ARCHIVE_DIRECTORY;
};

/**
 * @return {boolean} If the files with names starting with "." are not shown.
 */
DirectoryModel.prototype.isFilterHiddenOn = function() {
  return !!this.filters_['hidden'];
};

/**
 * @param {boolean} value Whether files with leading "." are hidden.
 */
DirectoryModel.prototype.setFilterHidden = function(value) {
  if (value) {
    this.addFilter('hidden',
                   function(e) {return e.name.substr(0, 1) != '.';});
  } else {
    this.removeFilter('hidden');
  }
};

/**
 * @return {DirectoryEntry} Current directory.
 */
DirectoryModel.prototype.getCurrentDirEntry = function() {
  return this.currentDirEntry_;
};

/**
 * @return {string} Path for the current directory.
 */
DirectoryModel.prototype.getCurrentDirPath = function() {
  return this.currentDirEntry_.fullPath;
};

/**
 * @private
 * @return {Array.<string>} Names of selected files.
 */
DirectoryModel.prototype.getSelectedNames_ = function() {
  var indexes = this.fileListSelection_.selectedIndexes;
  var dataModel = this.fileList_;
  if (dataModel) {
    return indexes.map(function(i) {
      return dataModel.item(i).name;
    });
  }
  return [];
};

/**
 * @private
 * @param {Array.<string>} value List of names of selected files.
 */
DirectoryModel.prototype.setSelectedNames_ = function(value) {
  var indexes = [];
  var dataModel = this.fileList_;

  function safeKey(key) {
    // The transformation must:
    // 1. Never generate a reserved name ('__proto__')
    // 2. Keep different keys different.
    return '#' + key;
  }

  var hash = {};

  for (var i = 0; i < value.length; i++)
    hash[safeKey(value[i])] = 1;

  for (var i = 0; i < dataModel.length; i++) {
    if (hash.hasOwnProperty(safeKey(dataModel.item(i).name)))
      indexes.push(i);
  }
  this.fileListSelection_.selectedIndexes = indexes;
};

/**
 * @private
 * @return {string} Lead item file name.
 */
DirectoryModel.prototype.getLeadName_ = function() {
  var index = this.fileListSelection_.leadIndex;
  return index >= 0 && this.fileList_.item(index).name;
};

/**
 * @private
 * @param {string} value The name of new lead index.
 */
DirectoryModel.prototype.setLeadName_ = function(value) {
  for (var i = 0; i < this.fileList_.length; i++) {
    if (this.fileList_.item(i).name == value) {
      this.fileListSelection_.leadIndex = i;
      return;
    }
  }
};

/**
 * @return {cr.ui.ArrayDataModel} The list of roots.
 */
DirectoryModel.prototype.getRootsList = function() {
  return this.rootsList_;
};

/**
 * @return {Entry} Directory entry of the current root.
 */
DirectoryModel.prototype.getCurrentRootDirEntry = function() {
  return this.rootsList_.item(this.rootsListSelection_.selectedIndex);
};

/**
 * @return {string} Root path for the current directory (parent directory is
 *     not navigatable for the user).
 */
DirectoryModel.prototype.getCurrentRootPath = function() {
  return DirectoryModel.getRootPath(this.currentDirEntry_.fullPath);
};

/**
 * @return {cr.ui.ListSingleSelectionModel} Root list selection model.
 */
DirectoryModel.prototype.getRootsListSelectionModel = function() {
  return this.rootsListSelection_;
};

/**
 * Add a filter for directory contents.
 * @param {string} name An identifier of the filter (used when removing it).
 * @param {function(Entry):boolean} filter Hide file if false.
 */
DirectoryModel.prototype.addFilter = function(name, filter) {
  this.filters_[name] = filter;
  this.rescanSoon();
};

/**
 * Remove one of the directory contents filters, specified by name.
 * @param {string} name Identifier of a filter.
 */
DirectoryModel.prototype.removeFilter = function(name) {
  delete this.filters_[name];
  this.rescanSoon();
};

/**
 * Schedule rescan with short delay.
 */
DirectoryModel.prototype.rescanSoon = function() {
  this.scheduleRescan(SHORT_RESCAN_INTERVAL);
};

/**
 * Schedule rescan with delay. Designed to handle directory change
 * notification.
 */
DirectoryModel.prototype.rescanLater = function() {
  this.scheduleRescan(SIMULTANEOUS_RESCAN_INTERVAL);
};

/**
 * Schedule rescan with delay. If another rescan has been scheduled does
 * nothing. File operation may cause a few notifications what should cause
 * a single refresh.
 * @param {number} delay Delay in ms after which the rescan will be performed.
 */
DirectoryModel.prototype.scheduleRescan = function(delay) {
  if (this.rescanTimeout_ && this.rescanTimeout_ <= delay)
    return;  // Rescan already scheduled.

  var self = this;
  function onTimeout() {
    self.rescanTimeout_ = undefined;
    self.rescan();
  }
  this.rescanTimeout_ = setTimeout(onTimeout, delay);
};

/**
 * Rescan current directory. May be called indirectly through rescanLater or
 * directly in order to reflect user action.
 */
DirectoryModel.prototype.rescan = function() {
  if (this.rescanTimeout_) {
    clearTimeout(this.rescanTimeout_);
    this.rescanTimeout_ = undefined;
  }

  var fileList = [];
  var successCallback = (function() {
    this.replaceFileList_(fileList);
    cr.dispatchSimpleEvent(this, 'rescan-completed');
  }).bind(this);

  if (this.runningScan_) {
    if (!this.pendingScan_)
      this.pendingScan_ = this.createScanner_(fileList, successCallback);
    return;
  }

  this.runningScan_ = this.createScanner_(fileList, successCallback);
  this.runningScan_.run();
};

/**
 * @private
 * @param {Array.<Entry>|cr.ui.ArrayDataModel} list File list.
 * @param {function} successCallback Callback on success.
 * @return {DirectoryModel.Scanner} New Scanner instance.
 */
DirectoryModel.prototype.createScanner_ = function(list, successCallback) {
  var self = this;
  function onSuccess() {
    self.scanFailures_ = 0;
    successCallback();
    if (self.pendingScan_) {
      self.runningScan_ = self.pendingScan_;
      self.pendingScan_ = null;
      self.runningScan_.run();
    } else {
      self.runningScan_ = null;
    }
  }

  function onFailure() {
    self.scanFailures_++;
    if (self.scanFailures_ <= 1)
      self.rescanLater();
  }

  return new DirectoryModel.Scanner(
      this.currentDirEntry_,
      list,
      onSuccess,
      onFailure,
      this.prefetchCacheForSorting_.bind(this),
      this.filters_);
};

/**
 * @private
 * @param {Array.<Entry>} entries List of files.
 */
DirectoryModel.prototype.replaceFileList_ = function(entries) {
  cr.dispatchSimpleEvent(this, 'begin-update-files');
  this.fileListSelection_.beginChange();

  var selectedNames = this.getSelectedNames_();
  // Restore leadIndex in case leadName no longer exists.
  var leadIndex = this.fileListSelection_.leadIndex;
  var leadName = this.getLeadName_();

  var spliceArgs = [].slice.call(entries);
  spliceArgs.unshift(0, this.fileList_.length);
  this.fileList_.splice.apply(this.fileList_, spliceArgs);

  this.setSelectedNames_(selectedNames);
  this.fileListSelection_.leadIndex = leadIndex;
  this.setLeadName_(leadName);
  this.fileListSelection_.endChange();
  cr.dispatchSimpleEvent(this, 'end-update-files');
};

/**
 * Cancels waiting and scheduled rescans and starts new scan.
 *
 * If the scan completes successfully on the first attempt, the callback will
 * be invoked and a 'scan-completed' event will be dispatched.  If the scan
 * fails for any reason, we'll periodically retry until it succeeds (and then
 * send a 'rescan-complete' event) or is cancelled or replaced by another
 * scan.
 *
 * @private
 * @param {function} callback Called if scan completes on the first attempt.
 *   Note that this will NOT be called if the scan fails but later succeeds.
 */
DirectoryModel.prototype.scan_ = function(callback) {
  if (this.rescanTimeout_) {
    clearTimeout(this.rescanTimeout_);
    this.rescanTimeout_ = 0;
  }
  if (this.runningScan_) {
    this.runningScan_.cancel();
    this.runningScan_ = null;
  }
  this.pendingScan_ = null;

  var onDone = function() {
    cr.dispatchSimpleEvent(this, 'scan-completed');
    callback();
  }.bind(this);

  // Clear the table first.
  this.fileList_.splice(0, this.fileList_.length);
  cr.dispatchSimpleEvent(this, 'scan-started');
  if (this.currentDirEntry_ == this.unmountedGDataEntry_) {
    onDone();
    return;
  }
  this.runningScan_ = this.createScanner_(this.fileList_, onDone);
  this.runningScan_.run();
};

/**
 * @private
 * @param {Array.<Entry>} entries Files.
 * @param {function} callback Callback on done.
 */
DirectoryModel.prototype.prefetchCacheForSorting_ = function(entries,
                                                             callback) {
  var field = this.fileList_.sortStatus.field;
  if (field) {
    this.prepareSortEntries_(entries, field, callback);
  } else {
    callback();
  }
};

/**
 * Delete the list of files and directories from filesystem and
 * update the file list.
 * @param {Array.<Entry>} entries Entries to delete.
 * @param {function()=} opt_callback Called when finished.
 */
DirectoryModel.prototype.deleteEntries = function(entries, opt_callback) {
  var downcount = entries.length + 1;

  var onComplete = opt_callback ? function() {
    if (--downcount == 0)
      opt_callback();
  } : function() {};

  var fileList = this.fileList_;
  for (var i = 0; i < entries.length; i++) {
    var entry = entries[i];

    var onSuccess = function(removedEntry) {
      var index = fileList.indexOf(removedEntry);
      if (index >= 0)
        fileList.splice(index, 1);
      onComplete();
    }.bind(null, entry);

    util.removeFileOrDirectory(
        entry,
        onSuccess,
        util.flog('Error deleting ' + entry.fullPath, onComplete));
  }
  onComplete();
};

/**
 * @param {string} name Filename.
 */
DirectoryModel.prototype.onEntryChanged = function(name) {
  var currentEntry = this.currentDirEntry_;
  var dm = this.fileList_;
  var self = this;

  function onEntryFound(entry) {
    self.prefetchCacheForSorting_([entry], function() {
      // Do nothing if current directory changed during async operations.
      if (self.currentDirEntry_ != currentEntry)
        return;

      var index = self.findIndexByName_(name);
      if (index >= 0)
        dm.splice(index, 1, entry);
      else
        dm.splice(dm.length, 0, entry);
    });
  };

  function onError(err) {
    // Do nothing if current directory changed during async operations.
    if (self.currentDirEntry_ != currentEntry)
      return;
    if (err.code != FileError.NOT_FOUND_ERR) {
      self.rescanLater();
      return;
    }

    var index = self.findIndexByName_(name);
    if (index >= 0)
      dm.splice(index, 1);
  };

  util.resolvePath(this.currentDirEntry_, name, onEntryFound, onError);
};

/**
 * @private
 * @param {string} name Filename.
 * @return {number} The index in the fileList.
 */
DirectoryModel.prototype.findIndexByName_ = function(name) {
  var dm = this.fileList_;
  for (var i = 0; i < dm.length; i++)
    if (dm.item(i).name == name)
      return i;
  return -1;
};

/**
 * Rename the entry in the filesystem and update the file list.
 * @param {Entry} entry Entry to rename.
 * @param {string} newName New name.
 * @param {function} errorCallback Called on error.
 * @param {function} opt_successCallback Called on success.
 */
DirectoryModel.prototype.renameEntry = function(entry, newName, errorCallback,
                                                opt_successCallback) {
  var self = this;
  function onSuccess(newEntry) {
    self.prefetchCacheForSorting_([newEntry], function() {
      var fileList = self.fileList_;
      var index = fileList.indexOf(entry);
      if (index >= 0)
        fileList.splice(index, 1, newEntry);
      self.selectEntry(newName);
      // If the entry doesn't exist in the list it mean that it updated from
      // outside (probably by directory rescan).
      if (opt_successCallback)
        opt_successCallback();
    });
  }
  entry.moveTo(this.currentDirEntry_, newName, onSuccess, errorCallback);
};

/**
 * Checks if current directory contains a file or directory with this name.
 * @param {string} newName Name to check.
 * @param {function(boolean, boolean?)} callback Called when the result's
 *     available. First parameter is true if the entry exists and second
 *     is true if it's a file.
 */
DirectoryModel.prototype.doesExist = function(newName, callback) {
  util.resolvePath(this.currentDirEntry_, newName,
      function(entry) {
        callback(true, entry.isFile);
      },
      callback.bind(window, false));
};

/**
 * Creates directory and updates the file list.
 *
 * @param {string} name Directory name.
 * @param {function} successCallback Callback on success.
 * @param {function} errorCallback Callback on failure.
 */
DirectoryModel.prototype.createDirectory = function(name, successCallback,
                                                    errorCallback) {
  var self = this;
  function onSuccess(newEntry) {
    self.prefetchCacheForSorting_([newEntry], function() {
      var fileList = self.fileList_;
      var existing = fileList.slice().filter(
          function(e) { return e.name == name; });

      if (existing.length) {
        self.selectEntry(name);
        successCallback(existing[0]);
      } else {
        self.fileListSelection_.beginChange();
        fileList.splice(0, 0, newEntry);
        self.selectEntry(name);
        self.fileListSelection_.endChange();
        successCallback(newEntry);
      }
    });
  }

  this.currentDirEntry_.getDirectory(name, {create: true, exclusive: true},
                                     onSuccess, errorCallback);
};

/**
 * Changes directory. Causes 'directory-change' event.
 *
 * @param {string} path New current directory path.
 */
DirectoryModel.prototype.changeDirectory = function(path) {
  this.resolveDirectory(path, function(directoryEntry) {
    this.changeDirectoryEntry_(false, directoryEntry);
  }.bind(this), function(error) {
    console.error('Error changing directory to ' + path + ': ', error);
  });
};

/**
 * Resolves absolute directory path. Handles GData stub.
 * @param {string} path Path to the directory.
 * @param {function(DirectoryEntry} successCallback Success callback.
 * @param {function(FileError} errorCallback Error callback.
 */
DirectoryModel.prototype.resolveDirectory = function(path, successCallback,
                                                     errorCallback) {
  if (this.unmountedGDataEntry_ &&
      DirectoryModel.getRootType(path) == DirectoryModel.RootType.GDATA) {
    // TODO(kaznacheeev): Currently if path points to some GData subdirectory
    // and GData is not mounted we will change to the fake GData root and
    // ignore the rest of the path. Consider remembering the path and
    // changing to it once GDdata is mounted. This is only relevant for cases
    // when we open the File Manager with an URL pointing to GData (e.g. via
    // a bookmark).
    successCallback(this.unmountedGDataEntry_);
    return;
  }

  if (path == '/') {
    successCallback(this.root_);
    return;
  }

  this.root_.getDirectory(path, {create: false},
                          successCallback, errorCallback);
};

/**
 * Changes directory. If path points to a root (except current one)
 * then directory changed to the last used one for the root.
 *
 * @param {string} path New current directory path or new root.
 */
DirectoryModel.prototype.changeDirectoryOrRoot = function(path) {
  if (DirectoryModel.getRootPath(path) == this.getCurrentRootPath()) {
    this.changeDirectory(path);
  } else if (this.currentDirByRoot_[path]) {
    this.resolveDirectory(
        this.currentDirByRoot_[path],
        this.changeDirectoryEntry_.bind(this, false),
        this.changeDirectory.bind(this, path));
  } else {
    this.changeDirectory(path);
  }
};

/**
 * Change the current directory to the directory represented by a
 * DirectoryEntry.
 *
 * Dispatches the 'directory-changed' event when the directory is successfully
 * changed.
 *
 * @private
 * @param {boolean} initial True if it comes from setupPath and
 *                          false if caused by an user action.
 * @param {DirectoryEntry} dirEntry The absolute path to the new directory.
 * @param {function} opt_callback Executed if the directory loads successfully.
 */
DirectoryModel.prototype.changeDirectoryEntry_ = function(initial, dirEntry,
                                                          opt_callback) {
  var previous = this.currentDirEntry_;
  this.currentDirEntry_ = dirEntry;
  function onRescanComplete() {
    if (opt_callback)
      opt_callback();
    // For tests that open the dialog to empty directories, everything
    // is loaded at this point.
    chrome.test.sendMessage('directory-change-complete');
  }
  this.updateRootsListSelection_();
  this.scan_(onRescanComplete);
  this.currentDirByRoot_[this.getCurrentRootPath()] = dirEntry.fullPath;

  var e = new cr.Event('directory-changed');
  e.previousDirEntry = previous;
  e.newDirEntry = dirEntry;
  e.initial = initial;
  this.dispatchEvent(e);
};

/**
 * Change the state of the model to reflect the specified path (either a
 * file or directory).
 *
 * @param {string} path The root path to use
 * @param {function=} opt_loadedCallback Invoked when the entire directory
 *     has been loaded and any default file selected.  If there are any
 *     errors loading the directory this will not get called (even if the
 *     directory loads OK on retry later). Will NOT be called if another
 *     directory change happened while setupPath was in progress.
 * @param {function=} opt_pathResolveCallback Invoked as soon as the path has
 *     been resolved, and called with the base and leaf portions of the path
 *     name, and a flag indicating if the entry exists. Will be called even
 *     if another directory change happened while setupPath was in progress,
 *     but will pass |false| as |exist| parameter.
 */
DirectoryModel.prototype.setupPath = function(path, opt_loadedCallback,
                                              opt_pathResolveCallback) {
  var overridden = false;
  function onExternalDirChange() { overridden = true }
  this.addEventListener('directory-changed', onExternalDirChange);

  var resolveCallback = function(exists) {
    this.removeEventListener('directory-changed', onExternalDirChange);
    if (opt_pathResolveCallback)
      opt_pathResolveCallback(baseName, leafName, exists && !overridden);
  }.bind(this);

  var changeDirectoryEntry = function(entry, initial, exists, opt_callback) {
    resolveCallback(exists);
    if (!overridden)
      this.changeDirectoryEntry_(initial, entry, opt_callback);
  }.bind(this);

  var INITIAL = true;
  var EXISTS = true;

  // Split the dirname from the basename.
  var ary = path.match(/^(?:(.*)\/)?([^\/]*)$/);

  if (!ary) {
    console.warn('Unable to split default path: ' + path);
    changeDirectoryEntry(this.root_, INITIAL, !EXISTS);
    return;
  }

  var baseName = ary[1];
  var leafName = ary[2];

  function onLeafFound(baseDirEntry, leafEntry) {
    if (leafEntry.isDirectory) {
      baseName = path;
      leafName = '';
      changeDirectoryEntry(leafEntry, INITIAL, EXISTS);
      return;
    }

    // Leaf is an existing file, cd to its parent directory and select it.
    changeDirectoryEntry(baseDirEntry,
                         !INITIAL /*HACK*/,
                         EXISTS,
                         function() {
                           this.selectEntry(leafEntry.name);
                           if (opt_loadedCallback)
                             opt_loadedCallback();
                         }.bind(this));
    // TODO(kaznacheev): Fix history.replaceState for the File Browser and
    // change !INITIAL to INITIAL. Passing |false| makes things
    // less ugly for now.
  }

  function onLeafError(baseDirEntry, err) {
    // Usually, leaf does not exist, because it's just a suggested file name.
    if (err.code != FileError.NOT_FOUND_ERR)
      console.log('Unexpected error resolving default leaf: ' + err);
    changeDirectoryEntry(baseDirEntry, INITIAL, !EXISTS);
  }

  var onBaseError = function(err) {
    console.log('Unexpected error resolving default base "' +
                baseName + '": ' + err);
    if (path != this.getDefaultDirectory()) {
      // Can't find the provided path, let's go to default one instead.
      resolveCallback(!EXISTS);
      if (!overridden)
        this.setupDefaultPath(opt_loadedCallback);
    } else {
      // Well, we can't find the downloads dir. Let's just show something,
      // or we will get an infinite recursion.
      changeDirectoryEntry(this.root_, opt_loadedCallback, INITIAL, !EXISTS);
    }
  }.bind(this);

  var onBaseFound = function(baseDirEntry) {
    if (!leafName) {
      // Default path is just a directory, cd to it and we're done.
      changeDirectoryEntry(baseDirEntry, INITIAL, !EXISTS);
      return;
    }

    util.resolvePath(this.root_, path,
                     onLeafFound.bind(this, baseDirEntry),
                     onLeafError.bind(this, baseDirEntry));
  }.bind(this);

  var root = this.root_;
  if (!baseName)
    baseName = this.getDefaultDirectory();
  root.getDirectory(baseName, {create: false}, onBaseFound, onBaseError);
};

/**
 * @param {function} opt_callback Callback on done.
 */
DirectoryModel.prototype.setupDefaultPath = function(opt_callback) {
  this.setupPath(this.getDefaultDirectory(), opt_callback);
};

/**
 * @return {string} The default directory.
 */
DirectoryModel.prototype.getDefaultDirectory = function() {
  return '/' + DirectoryModel.DOWNLOADS_DIRECTORY;
};

/**
 * @param {string} name Filename.
 */
DirectoryModel.prototype.selectEntry = function(name) {
  var dm = this.fileList_;
  for (var i = 0; i < dm.length; i++) {
    if (dm.item(i).name == name) {
      this.selectIndex(i);
      return;
    }
  }
};

/**
 * @param {number} index Index of file.
 */
DirectoryModel.prototype.selectIndex = function(index) {
  // this.focusCurrentList_();
  if (index >= this.fileList_.length)
    return;

  // If a list bound with the model it will do scrollIndexIntoView(index).
  this.fileListSelection_.selectedIndex = index;
};

/**
 * Cache necessary data before a sort happens.
 *
 * This is called by the table code before a sort happens, so that we can
 * go fetch data for the sort field that we may not have yet.
 * @private
 * @param {string} field Sort field.
 * @param {function} callback Called when done.
 */
DirectoryModel.prototype.prepareSort_ = function(field, callback) {
  this.prepareSortEntries_(this.fileList_.slice(), field, callback);
};

/**
 * @private
 * @param {Array.<Entry>} entries Files.
 * @param {string} field Sort field.
 * @param {function} callback Called when done.
 */
DirectoryModel.prototype.prepareSortEntries_ = function(entries, field,
                                                        callback) {
  this.metadataCache_.get(entries, 'filesystem', function(properties) {
    callback();
  });
};

/**
 * Get root entries asynchronously.
 * @private
 * @param {function(Array.<Entry>)} callback Called when roots are resolved.
 * @param {boolean} resolveGData See comment for updateRoots.
 */
DirectoryModel.prototype.resolveRoots_ = function(callback, resolveGData) {
  var groups = {
    downloads: null,
    archives: null,
    removables: null,
    gdata: null
  };
  var self = this;

  metrics.startInterval('Load.Roots');
  function done() {
    for (var i in groups)
      if (!groups[i])
        return;

    self.updateVolumeReadOnlyStatus_(groups.removables);
    callback(groups.downloads.
             concat(groups.gdata).
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

  function onGData(entry) {
    console.log('GData found:', entry);
    self.unmountedGDataEntry_ = null;
    groups.gdata = [entry];
    done();
  }

  function onGDataError(error) {
    console.log('GData error: ' + error);
    self.unmountedGDataEntry_ = {
      unmounted: true,  // Clients use this field to distinguish a fake root.
      toURL: function() { return '' },
      fullPath: '/' + DirectoryModel.GDATA_DIRECTORY
    };
    groups.gdata = [self.unmountedGDataEntry_];
    done();
  }

  var root = this.root_;
  root.getDirectory(DirectoryModel.DOWNLOADS_DIRECTORY, { create: false },
                    onDownloads, onDownloadsError);
  util.readDirectory(root, DirectoryModel.ARCHIVE_DIRECTORY,
                     append.bind(this, 'archives'));
  util.readDirectory(root, DirectoryModel.REMOVABLE_DIRECTORY,
                     append.bind(this, 'removables'));
  if (this.showGData_) {
    if (resolveGData) {
      root.getDirectory(DirectoryModel.GDATA_DIRECTORY, { create: false },
                        onGData, onGDataError);
    } else {
      onGDataError('lazy mount');
    }
  } else {
    groups.gdata = [];
  }
};

/**
* @param {function} opt_callback Called when all roots are resolved.
* @param {boolean} opt_resolveGData If true GData should be resolved for real,
*                                   If false a stub entry should be created.
*/
DirectoryModel.prototype.updateRoots = function(opt_callback,
                                                opt_resolveGData) {
  var self = this;
  this.resolveRoots_(function(rootEntries) {
    var dm = self.rootsList_;
    var args = [0, dm.length].concat(rootEntries);
    dm.splice.apply(dm, args);

    self.updateRootsListSelection_();

    if (opt_callback)
      opt_callback();
  }, opt_resolveGData);
};

/**
 * Change root. In case user already opened the root, the last selected
 * directory will be selected.
 * @param {string} rootPath The path of the root.
 */
DirectoryModel.prototype.changeRoot = function(rootPath) {
  if (this.currentDirByRoot_[rootPath]) {
    var onFail = this.changeDirectory.bind(this, rootPath);
    this.changeDirectory(this.currentDirByRoot_[rootPath], onFail);
  } else {
    this.changeDirectory(rootPath);
  }
};

/**
 * Find roots list item by root path.
 *
 * @param {string} path Root path.
 * @return {number} Index of the item.
 * @private
 */
DirectoryModel.prototype.findRootsListItem_ = function(path) {
  var roots = this.rootsList_;
  for (var index = 0; index < roots.length; index++) {
    if (roots.item(index).fullPath == path)
      return index;
  }
  return -1;
};

/**
 * @private
 */
DirectoryModel.prototype.updateRootsListSelection_ = function() {
  var rootPath = DirectoryModel.getRootPath(this.currentDirEntry_.fullPath);
  this.rootsListSelection_.selectedIndex = this.findRootsListItem_(rootPath);
};

/**
 * @param {Array.<DirectoryEntry>} roots Removable volumes entries.
 * @private
 */
DirectoryModel.prototype.updateVolumeReadOnlyStatus_ = function(roots) {
  var status = this.volumeReadOnlyStatus_ = {};
  for (var i = 0; i < roots.length; i++) {
    status[roots[i].fullPath] = false;
    chrome.fileBrowserPrivate.getVolumeMetadata(roots[i].toURL(),
        function(systemMetadata, path) {
          status[path] = !!(systemMetadata && systemMetadata.isReadOnly);
        }.bind(null, roots[i].fullPath));
  }
};

/**
 * Prepare the root for the unmount.
 *
 * @param {string} rootPath The path to the root.
 */
DirectoryModel.prototype.prepareUnmount = function(rootPath) {
  var index = this.findRootsListItem_(rootPath);
  if (index == -1) {
    console.error('Unknown root entry', rootPath);
    return;
  }
  var entry = this.rootsList_.item(index);

  // We never need to remove this attribute because even if the unmount fails
  // the onMountCompleted handler calls updateRoots which creates a new entry
  // object for this volume.
  entry.unmounting = true;

  // Re-place the entry into the roots data model to force re-rendering.
  this.rootsList_.splice(index, 1, entry);

  if (rootPath == this.rootPath) {
    // TODO(kaznacheev): Consider changing to the most recently used root.
    this.changeDirectory(this.getDefaultDirectory());
  }
};

/**
 * @param {string} path Any path.
 * @return {string} The root path.
 */
DirectoryModel.getRootPath = function(path) {
  var type = DirectoryModel.getRootType(path);

  if (type == DirectoryModel.RootType.DOWNLOADS)
    return '/' + DirectoryModel.DOWNLOADS_DIRECTORY;
  if (type == DirectoryModel.RootType.GDATA)
    return '/' + DirectoryModel.GDATA_DIRECTORY;

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

/**
 * @param {string} path Any path.
 * @return {string} The name of the root.
 */
DirectoryModel.getRootName = function(path) {
  var root = DirectoryModel.getRootPath(path);
  var index = root.lastIndexOf('/');
  return index == -1 ? root : root.substring(index + 1);
};

/**
 * @param {string} path A path.
 * @return {string} A root type.
 */
DirectoryModel.getRootType = function(path) {
  function isTop(dir) {
    return path.substr(1, dir.length) == dir;
  }

  if (isTop(DirectoryModel.DOWNLOADS_DIRECTORY))
    return DirectoryModel.RootType.DOWNLOADS;
  if (isTop(DirectoryModel.GDATA_DIRECTORY))
    return DirectoryModel.RootType.GDATA;
  if (isTop(DirectoryModel.ARCHIVE_DIRECTORY))
    return DirectoryModel.RootType.ARCHIVE;
  if (isTop(DirectoryModel.REMOVABLE_DIRECTORY))
    return DirectoryModel.RootType.REMOVABLE;
  return '';
};

/**
 * @param {string} path A path.
 * @return {boolean} True if it is a path to the root.
 */
DirectoryModel.isRootPath = function(path) {
  if (path[path.length - 1] == '/')
    path = path.substring(0, path.length - 1);
  return DirectoryModel.getRootPath(path) == path;
};

/**
 * @constructor
 * @extends cr.EventTarget
 * @param {DirectoryEntry} dir Directory to scan.
 * @param {Array.<Entry>|cr.ui.ArrayDataModel} list Target to put the files.
 * @param {function} successCallback Callback to call when (and if) scan
 *     successfully completed.
 * @param {function} errorCallback Callback to call in case of IO error.
 * @param {function(Array.<Entry>):void, Function)} preprocessChunk
 *     Callback to preprocess each chunk of files.
 * @param {Object.<string, function(Entry):Boolean>} filters The map of filters
 *     for file entries.
 */
DirectoryModel.Scanner = function(dir, list, successCallback, errorCallback,
                                  preprocessChunk, filters) {
  this.cancelled_ = false;
  this.list_ = list;
  this.dir_ = dir;
  this.reader_ = null;
  this.filters_ = filters;
  this.preprocessChunk_ = preprocessChunk;
  this.successCallback_ = successCallback;
  this.errorCallback_ = errorCallback;
};

/**
 * Extends EventTarget.
 */
DirectoryModel.Scanner.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Cancel scanner.
 */
DirectoryModel.Scanner.prototype.cancel = function() {
  this.cancelled_ = true;
};

/**
 * Start scanner.
 */
DirectoryModel.Scanner.prototype.run = function() {
  metrics.startInterval('DirectoryScan');

  this.reader_ = this.dir_.createReader();
  this.readNextChunk_();
};

/**
 * @private
 */
DirectoryModel.Scanner.prototype.readNextChunk_ = function() {
  this.reader_.readEntries(this.onChunkComplete_.bind(this),
                           this.errorCallback_);
};

/**
 * @private
 * @param {Array.<Entry>} entries File list.
 */
DirectoryModel.Scanner.prototype.onChunkComplete_ = function(entries) {
  if (this.cancelled_)
    return;

  if (entries.length == 0) {
    this.successCallback_();
    this.recordMetrics_();
    return;
  }

  // Splice takes the to-be-spliced-in array as individual parameters,
  // rather than as an array, so we need to perform some acrobatics...
  var spliceArgs = [].slice.call(entries);

  for (var filterName in this.filters_) {
    spliceArgs = spliceArgs.filter(this.filters_[filterName]);
  }

  var self = this;
  self.preprocessChunk_(spliceArgs, function() {
    spliceArgs.unshift(0, 0);  // index, deleteCount
    self.list_.splice.apply(self.list_, spliceArgs);

    // Keep reading until entries.length is 0.
    self.readNextChunk_();
  });
};

/**
 * @private
 */
DirectoryModel.Scanner.prototype.recordMetrics_ = function() {
  metrics.recordInterval('DirectoryScan');
  if (this.dir_.fullPath ==
      '/' + DirectoryModel.DOWNLOADS_DIRECTORY) {
    metrics.recordMediumCount('DownloadsCount', this.list_.length);
  }
};
