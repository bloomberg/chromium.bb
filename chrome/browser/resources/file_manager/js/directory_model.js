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
 * @param {MetadataCache} metadataCache The metadata cache service.
 * @param {VolumeManager} volumeManager The volume manager.
 * @param {boolean} isGDataEnabled True if GDATA enabled (initial value).
 */
function DirectoryModel(root, singleSelection,
                        metadataCache, volumeManager, isGDataEnabled) {
  this.root_ = root;
  this.metadataCache_ = metadataCache;
  this.fileList_ = new cr.ui.ArrayDataModel([]);
  this.fileListSelection_ = singleSelection ?
      new cr.ui.ListSingleSelectionModel() : new cr.ui.ListSelectionModel();

  this.runningScan_ = null;
  this.pendingScan_ = null;
  this.rescanTimeout_ = undefined;
  this.scanFailures_ = 0;
  this.gDataEnabled_ = isGDataEnabled;

  // DirectoryEntry representing the current directory of the dialog.
  this.currentDirEntry_ = root;

  this.fileList_.prepareSort = this.prepareSort_.bind(this);

  this.rootsList_ = new cr.ui.ArrayDataModel([]);
  this.rootsListSelection_ = new cr.ui.ListSingleSelectionModel();
  this.rootsListSelection_.addEventListener(
      'change', this.onRootChange_.bind(this));

  /**
   * A map root.fullPath -> currentDirectory.fullPath.
   * @private
   * @type {Object.<string, string>}
   */
  this.currentDirByRoot_ = {};

  // The map 'name' -> callback. Callbacks are function(entry) -> boolean.
  this.filters_ = {};
  this.setFilterHidden(true);

  this.volumeManager_ = volumeManager;

  /**
   * Directory in which search results are displayed. Not null iff search
   * results are being displayed.
   * @private
   * @type {Entry}
   */
  this.searchDirEntry_ = null;

  /**
   * Is search in progress.
   * @private
   * @type {boolean}
   */
  this.isSearching_ = false;
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
 * Fake entry to be used in currentDirEntry_ when current directory is
 * unmounted GDATA.
 * @private
 */
DirectoryModel.fakeGDataEntry_ = {
  fullPath: '/' + DirectoryModel.GDATA_DIRECTORY
};

/**
 * Root path used for displaying gdata content search results.
 * Search results will be shown in directory 'GDATA_SEARCH_ROOT_PATH/query'.
 *
 * @const
 * @type {string}
 */
DirectoryModel.GDATA_SEARCH_ROOT_PATH = '/drive/.search';

/**
 * @const
 * @type {Array.<string>}
 */
DirectoryModel.GDATA_SEARCH_ROOT_COMPONENTS = ['', 'drive', '.search'];

/**
 * DirectoryModel extends cr.EventTarget.
 */
DirectoryModel.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Fills the root list and starts tracking changes.
 */
DirectoryModel.prototype.start = function() {
  var volumesChangeHandler = this.onMountChanged_.bind(this);
  this.volumeManager_.addEventListener('change', volumesChangeHandler);
  this.updateRoots_();
};

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
 * Sets whether GDATA appears in the roots list and
 * if it could be used as current directory.
 * @param {boolead} enabled True if GDATA enabled.
 */
DirectoryModel.prototype.setGDataEnabled = function(enabled) {
  if (this.gDataEnabled_ == enabled)
    return;
  this.gDataEnabled_ = enabled;
  this.updateRoots_();
  if (!enabled && this.getCurrentRootType() == DirectoryModel.RootType.GDATA)
    this.changeDirectory(this.getDefaultDirectory());
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
 * @return {boolean} True if search is in progress.
 */
DirectoryModel.prototype.isSearching = function() {
  return this.isSearching_;
};

/**
 * @return {boolean} True if we are currently showing search results.
 */
DirectoryModel.prototype.isOnGDataSearchDir = function() {
  return this.getSearchOrCurrentDirEntry() != this.getCurrentDirEntry();
};

/**
 * @param {strin} path Path to check.
 * @return {boolean} True if the |path| is read only.
 */
DirectoryModel.prototype.isPathReadOnly = function(path) {
  switch (DirectoryModel.getRootType(path)) {
    case DirectoryModel.RootType.REMOVABLE:
      return !!this.volumeManager_.isReadOnly(DirectoryModel.getRootPath(path));
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
 * If search results are being displayed, returns search directory, else returns
 * current directory.
 *
 * @return {DirectoryEntry} search or directory entry.
 */
DirectoryModel.prototype.getSearchOrCurrentDirEntry = function() {
  return this.searchDirEntry_ || this.currentDirEntry_;
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
 * @return {DirectoryModel.RootType} A root type.
 */
DirectoryModel.prototype.getCurrentRootType = function() {
  return DirectoryModel.getRootType(this.currentDirEntry_.fullPath);
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
  if (this.filters_[name])
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
      this.getSearchOrCurrentDirEntry(),
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

  var onDone = function() {
    cr.dispatchSimpleEvent(this, 'scan-completed');
    callback();
  }.bind(this);

  // Clear the table first.
  this.fileList_.splice(0, this.fileList_.length);
  cr.dispatchSimpleEvent(this, 'scan-started');
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
 * Gets name that should be displayed in the UI for the entry.
 * @param {string} path Full path of the entry whose display name we are
 *                      getting.
 * @param {string} defaultName Default name to use if no name is calculated.
 * @return {string} Name to be used for display.
 */
DirectoryModel.prototype.getDisplayName = function(path, defaultName) {
  var searchResultName = util.getFileAndDisplayNameForGDataSearchResult(path);
  return searchResultName ? searchResultName.displayName : defaultName;
};

/**
 * Creates file name that should be used as a new file name in filesystem
 * operations while renaming. If the given entry is not a gdata search result
 * entry, |displayName| will be used.
 *
 * @private
 * @param {Entry} entry Entry which is being renamed.
 * @param {string} displayName The new file name provided by user.
 * @return {string} File name that should be used in renaming filesystem
 *                 operations.
 */
DirectoryModel.prototype.getEntryNameForRename_ = function(entry, displayName) {
  // If we are renaming gdata search result, we'll have to format newName to
  // use in file system operation like: <resource_id>.<file_name>.
  var searchResultName =
      util.getFileAndDisplayNameForGDataSearchResult(entry.fullPath);
  return searchResultName ? searchResultName.resourceId + '.' + displayName :
                            displayName;
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
  var currentEntry = this.getSearchOrCurrentDirEntry();
  if (currentEntry != this.currentDirEntry_)
    return;
  var dm = this.fileList_;
  var self = this;

  function onEntryFound(entry) {
    // Do nothing if current directory changed during async operations.
    if (self.getSearchOrCurrentDirEntry() != currentEntry)
      return;
    self.prefetchCacheForSorting_([entry], function() {
      // Do nothing if current directory changed during async operations.
      if (self.getSearchOrCurrentDirEntry() != currentEntry)
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

  util.resolvePath(currentEntry, name, onEntryFound, onError);
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
 * @param {string} newDisplayName New name.
 * @param {function} errorCallback Called on error.
 * @param {function} opt_successCallback Called on success.
 */
DirectoryModel.prototype.renameEntry = function(entry, newDisplayName,
                                                errorCallback,
                                                opt_successCallback) {
  var self = this;
  function onSuccess(newEntry) {
    self.prefetchCacheForSorting_([newEntry], function() {
      var index = self.findIndexByName_(entry.name);
      if (index >= 0)
        self.fileList_.splice(index, 1, newEntry);
      self.selectEntry(newEntry.name);
      // If the entry doesn't exist in the list it mean that it updated from
      // outside (probably by directory rescan).
      if (opt_successCallback)
        opt_successCallback();
    });
  }

  var newEntryName = this.getEntryNameForRename_(entry, newDisplayName);
  entry.moveTo(this.getSearchOrCurrentDirEntry(), newEntryName, onSuccess,
               errorCallback);
};

/**
 * Checks if current directory contains a file or directory with this name.
 * @param {string} entry Entry to which newName will be given.
 * @param {string} displayName Name to check.
 * @param {function(boolean, boolean?)} callback Called when the result's
 *     available. First parameter is true if the entry exists and second
 *     is true if it's a file.
 */
DirectoryModel.prototype.doesExist = function(entry, displayName, callback) {
  var entryName = this.getEntryNameForRename_(entry, displayName);

  util.resolvePath(this.getSearchOrCurrentDirEntry(), entryName,
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
  var targetPath = path;
  // We should not be changing directory to gdata search path. If we do, default
  // to gdata root.
  if (DirectoryModel.isGDataSearchPath(path)) {
    console.error('Attempt to change directory to search path.');
    targetPath = '/' + DirectoryModel.GDATA_DIRECTORY;
  }

  this.resolveDirectory(targetPath, function(directoryEntry) {
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
  if (DirectoryModel.getRootType(path) == DirectoryModel.RootType.GDATA) {
    if (!this.isGDataMounted_()) {
      if (path == DirectoryModel.fakeGDataEntry_.fullPath)
        successCallback(DirectoryModel.fakeGDataEntry_);
      else  // Subdirectory.
        errorCallback({ code: FileError.NOT_FOUND_ERR });
      return;
    }
  }

  if (path == '/') {
    successCallback(this.root_);
    return;
  }

  this.root_.getDirectory(path, {create: false},
                          successCallback, errorCallback);
};

/**
 * Handler for root item being clicked.
 * @private
 * @param {Entry} entry Entry to navigate to.
 * @param {Event} event The event.
 */
DirectoryModel.prototype.onRootChange_ = function(entry, event) {
  var newRootDir = this.getCurrentRootDirEntry();
  if (newRootDir)
    this.changeRoot(newRootDir.fullPath);
};

/**
 * Changes directory. If path points to a root (except current one)
 * then directory changed to the last used one for the root.
 *
 * @param {string} path New current directory path or new root.
 */
DirectoryModel.prototype.changeRoot = function(path) {
  if (this.getCurrentRootPath() == path)
    return;
  if (this.currentDirByRoot_[path]) {
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
  if (dirEntry == DirectoryModel.fakeGDataEntry_)
    this.volumeManager_.mountGData(function() {}, function() {});

  this.clearSearch_();
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
 * Creates an object wich could say wether directory has changed while it has
 * been active or not. Designed for long operations that should be canncelled
 * if the used change current directory.
 * @return {Object} Created object.
 */
DirectoryModel.prototype.createDirectoryChangeTracker = function() {
  var tracker = {
    dm_: this,
    active_: false,
    hasChanged: false,
    exceptInitialChange: false,

    start: function() {
      if (!this.active_) {
        this.dm_.addEventListener('directory-changed',
                                  this.onDirectoryChange_);
        this.active_ = true;
        this.hasChanged = false;
      }
    },

    stop: function() {
      if (this.active_) {
        this.dm_.removeEventListener('directory-changed',
                                     this.onDirectoryChange_);
        active_ = false;
      }
    },

    onDirectoryChange_: function(event) {
      // this == tracker.dm_ here.
      if (tracker.exceptInitialChange && event.initial)
        return;
      tracker.stop();
      tracker.hasChanged = true;
    }
  };
  return tracker;
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
  var tracker = this.createDirectoryChangeTracker();
  tracker.start();

  var self = this;
  function resolveCallback(directoryPath, fileName, exists) {
    tracker.stop();
    if (!opt_pathResolveCallback)
      return;
    opt_pathResolveCallback(directoryPath, fileName,
                            exists && !tracker.hasChanged);
  }

  function changeDirectoryEntry(directoryEntry, initial, opt_callback) {
    tracker.stop();
    if (!tracker.hasChanged)
      self.changeDirectoryEntry_(initial, directoryEntry, opt_callback);
  }

  var INITIAL = true;
  var EXISTS = true;

  function changeToDefault() {
    var def = self.getDefaultDirectory();
    self.resolveDirectory(def, function(directoryEntry) {
      resolveCallback(def, '', !EXISTS);
      changeDirectoryEntry(directoryEntry, INITIAL);
    }, function(error) {
      console.error('Failed to resolve default directory: ' + def, error);
      resolveCallback('/', '', !EXISTS);
    });
  }

  function noParentDirectory(error) {
    console.log('Can\'t resolve parent directory: ' + path, error);
    changeToDefault();
  }

  if (DirectoryModel.isSystemDirectory(path)) {
    changeToDefault();
    return;
  }

  this.resolveDirectory(path, function(directoryEntry) {
    resolveCallback(directoryEntry.fullPath, '', !EXISTS);
    changeDirectoryEntry(directoryEntry, INITIAL);
  }, function(error) {
    // Usually, leaf does not exist, because it's just a suggested file name.
    var fileExists = error.code == FileError.TYPE_MISMATCH_ERR;
    if (fileExists || error.code == FileError.NOT_FOUND_ERR) {
      var nameDelimiter = path.lastIndexOf('/');
      var parentDirectoryPath = path.substr(0, nameDelimiter);
      if (DirectoryModel.isSystemDirectory(parentDirectoryPath)) {
        changeToDefault();
        return;
      }
      self.resolveDirectory(parentDirectoryPath,
                            function(parentDirectoryEntry) {
        var fileName = path.substr(nameDelimiter + 1);
        resolveCallback(parentDirectoryEntry.fullPath, fileName, fileExists);
        changeDirectoryEntry(parentDirectoryEntry,
                             !INITIAL /*HACK*/,
                             function() {
                               self.selectEntry(fileName);
                               if (opt_loadedCallback)
                                 opt_loadedCallback();
                             });
        // TODO(kaznacheev): Fix history.replaceState for the File Browser and
        // change !INITIAL to INITIAL. Passing |false| makes things
        // less ugly for now.
      }, noParentDirectory);
    } else {
      // Unexpected errors.
      console.error('Directory resolving error: ', error);
      changeToDefault();
    }
  });
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
 */
DirectoryModel.prototype.resolveRoots_ = function(callback) {
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

  function appendSingle(index, entry) {
    groups[index] = [entry];
    done();
  }

  function onSingleError(index, error, defaultValue) {
    groups[index] = defaultValue || [];
    done();
  }

  var root = this.root_;
  function readSingle(dir, index, opt_defaultValue) {
    root.getDirectory(dir, { create: false },
                      appendSingle.bind(this, index),
                      onSingleError.bind(this, index, opt_defaultValue));
  }

  readSingle(DirectoryModel.DOWNLOADS_DIRECTORY, 'downloads');
  util.readDirectory(root, DirectoryModel.ARCHIVE_DIRECTORY,
                     append.bind(this, 'archives'));
  util.readDirectory(root, DirectoryModel.REMOVABLE_DIRECTORY,
                     append.bind(this, 'removables'));

  if (this.gDataEnabled_) {
    var fake = [DirectoryModel.fakeGDataEntry_];
    if (this.isGDataMounted_())
      readSingle(DirectoryModel.GDATA_DIRECTORY, 'gdata', fake);
    else
      groups.gdata = fake;
  } else {
    groups.gdata = [];
  }
};

/**
 * Updates the roots list.
 * @private
 */
DirectoryModel.prototype.updateRoots_ = function() {
  var self = this;
  this.resolveRoots_(function(rootEntries) {
    var dm = self.rootsList_;
    var args = [0, dm.length].concat(rootEntries);
    dm.splice.apply(dm, args);

    self.updateRootsListSelection_();
  });
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
 * @return {true} True if GDATA mounted.
 * @private
 */
DirectoryModel.prototype.isGDataMounted_ = function() {
  return this.volumeManager_.isMounted('/' + DirectoryModel.GDATA_DIRECTORY);
};

/**
 * Handler for the VolumeManager's event.
 * @private
 */
DirectoryModel.prototype.onMountChanged_ = function() {
  this.updateRoots_();

  var rootType = this.getCurrentRootType();

  if ((rootType == DirectoryModel.RootType.ARCHIVE ||
       rootType == DirectoryModel.RootType.REMOVABLE) &&
      !this.volumeManager_.isMounted(this.getCurrentRootPath())) {
    this.changeDirectory(this.getDefaultDirectory());
  }

  if (rootType != DirectoryModel.RootType.GDATA)
    return;

  var mounted = this.isGDataMounted_();
  if (this.currentDirEntry_ == DirectoryModel.fakeGDataEntry_) {
    if (mounted) {
      // Change fake entry to real one and rescan.
      function onGotDirectory(entry) {
        if (this.currentDirEntry_ == DirectoryModel.fakeGDataEntry_) {
          this.currentDirEntry_ = entry;
          this.rescan();
        }
      }
      this.root_.getDirectory('/' + DirectoryModel.GDATA_DIRECTORY, {},
                              onGotDirectory.bind(this));
    }
  } else if (!mounted) {
    // Current entry unmounted. replace with fake one.
    if (this.currentDirEntry_.fullPath ==
        DirectoryModel.fakeGDataEntry_.fullPath) {
      // Replace silently and rescan.
      this.currentDirEntry_ = DirectoryModel.fakeGDataEntry_;
      this.rescan();
    } else {
      this.changeDirectoryEntry_(false, DirectoryModel.fakeGDataEntry_);
    }
  }
};

/**
 * @param {string} path Path
 * @return {boolean} If current directory is system.
 */
DirectoryModel.isSystemDirectory = function(path) {
  path = path.replace(/\/+$/, '');
  return path == '/' + DirectoryModel.REMOVABLE_DIRECTORY ||
         path == '/' + DirectoryModel.ARCHIVE_DIRECTORY;
};

/**
 * Performs search and displays results. The search type is dependent on the
 * current directory. If we are currently on gdata, server side content search
 * over gdata mount point. If the current directory is not on the gdata, file
 * name search over current directory wil be performed.
 *
 * @param {string} query Query that will be searched for.
 * @param {function} onSearchRescan Function that will be called when the search
 *     directory is rescanned (i.e. search results are displayed)
 * @param {function} onClearSearch Function to be called when search state gets
 *     cleared.
 */
DirectoryModel.prototype.search = function(query,
                                           onSearchRescan,
                                           onClearSearch) {
  if (!query) {
    if (this.isSearching_)
      this.clearSearch_();
    return;
  }

  this.isSearching_ = true;

  // If we alreaqdy have event listener for an old search, we have to remove it.
  if (this.onSearchRescan_)
    this.removeEventListener('rescan-completed', this.onSearchRescan_);

  this.onSearchRescan_ = onSearchRescan;
  this.onClearSearch_ = onClearSearch;

  this.addEventListener('rescan-completed', this.onSearchRescan_);

  // If we are offline, let's fallback to file name search inside dir.
  if (this.getRootType() == DirectoryModel.RootType.GDATA &&
      !this.isOffline()) {
    var self = this;
    // Create shadow directory which will contain search results.
    this.root_.getDirectory(DirectoryModel.createGDataSearchPath(query),
        {create: false},
        function(dir) {
          self.searchDirEntry_ = dir;
          self.rescanSoon();
        },
        function() {
          self.isSearching_ = false;
        });
  } else {
    var queryLC = query.toLowerCase();
    this.searchDirEntry_ = this.currentDirEntry_;
    this.addFilter(
        'searchbox',
        function(e) {
          return e.name.toLowerCase().indexOf(queryLC) > -1;
        });
  }
};


/**
 * Clears any state set by previous searches.
 * @private
 */
DirectoryModel.prototype.clearSearch_ = function() {
  if (!this.isSearching_)
    return;
  this.searchDirEntry_ = null;
  this.isSearching_ = false;
  // This will trigger rescan.
  this.removeFilter('searchbox');

  if (this.onSearchRescan_) {
    this.removeEventListener('rescan-completed', this.onSearchRescan_);
    this.onSearchRescan_ = null;
  }

  if (this.onClearSearch_) {
    this.onClearSearch_();
    this.onClearSearch_ = null;
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
 * @return {DirectoryModel.RootType} A root type.
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
 * Checks if the provided path is under gdata search.
 *
 * @param {string} path Path to be tested.
 * @return {boolean} Is the path gdata search path.
 */
DirectoryModel.isGDataSearchPath = function(path) {
  return path == DirectoryModel.GDATA_SEARCH_ROOT_PATH ||
         path.indexOf(DirectoryModel.GDATA_SEARCH_ROOT_PATH + '/') == 0;
};

/**
 * Creates directory path in which gdata content search results for |query|
 * should be displayed.
 *
 * @param {string} query Search query.
 * @return {string} Virtual directory path for search results.
 */
DirectoryModel.createGDataSearchPath = function(query) {
  return DirectoryModel.GDATA_SEARCH_ROOT_PATH + '/' + query;
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
  if (this.dir_ == DirectoryModel.fakeGDataEntry_) {
    if (!this.cancelled_)
      this.successCallback_();
    return;
  }

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

/**
 * @constructor
 * @param {DirectoryEntry} root Root entry.
 * @param {DirectoryModel} directoryModel Model to watch.
 * @param {VolumeManager} volumeManager Manager to watch.
 */
function FileWatcher(root, directoryModel, volumeManager) {
  this.root_ = root;
  this.dm_ = directoryModel;
  this.vm_ = volumeManager;
  this.watchedDirectoryEntry_ = null;
  this.updateWatchedDirectoryBound_ =
      this.updateWatchedDirectory_.bind(this);
  this.onFileChangedBound_ =
      this.onFileChanged_.bind(this);
}

/**
 * Starts watching.
 */
FileWatcher.prototype.start = function() {
  chrome.fileBrowserPrivate.onFileChanged.addListener(
        this.onFileChangedBound_);

  this.dm_.addEventListener('directory-changed',
      this.updateWatchedDirectoryBound_);
  this.vm_.addEventListener('changed',
      this.updateWatchedDirectoryBound_);

  this.updateWatchedDirectory_();
};

/**
 * Stops watching (must be called before page unload).
 */
FileWatcher.prototype.stop = function() {
  chrome.fileBrowserPrivate.onFileChanged.removeListener(
        this.onFileChangedBound_);

  this.dm_.removeEventListener('directory-changed',
      this.updateWatchedDirectoryBound_);
  this.vm_.removeEventListener('changed',
      this.updateWatchedDirectoryBound_);

  if (this.watchedDirectoryEntry_)
    this.changeWatchedEntry(null);
};

/**
 * @param {Object} event chrome.fileBrowserPrivate.onFileChanged event.
 * @private
 */
FileWatcher.prototype.onFileChanged_ = function(event) {
  if (encodeURI(event.fileUrl) == this.watchedDirectoryEntry_.toURL())
    this.onFileInWatchedDirectoryChanged();
};

/**
 * Called when file in the watched directory changed.
 */
FileWatcher.prototype.onFileInWatchedDirectoryChanged = function() {
  this.dm_.rescanLater();
};

/**
 * Called when directory changed or volumes mounted/unmounted.
 * @private
 */
FileWatcher.prototype.updateWatchedDirectory_ = function() {
  var current = this.watchedDirectoryEntry_;
  switch (this.dm_.getCurrentRootType()) {
    case DirectoryModel.RootType.GDATA:
      if (!this.vm_.isMounted('/' + DirectoryModel.GDATA_DIRECTORY))
         break;
    case DirectoryModel.RootType.DOWNLOADS:
    case DirectoryModel.RootType.REMOVABLE:
      if (!current || current.fullPath != this.dm_.getCurrentDirPath()) {
        // TODO(serya): Changed in readonly removable directoried don't
        //              need to be tracked.
        this.root_.getDirectory(this.dm_.getCurrentDirPath(), {},
                                this.changeWatchedEntry.bind(this),
                                this.changeWatchedEntry.bind(this, null));
      }
      return;
  }
  if (current)
    this.changeWatchedEntry(null);
};

/**
 * @param {Entry?} entry Null if no directory need to be watched or
 *                       directory to watch.
 */
FileWatcher.prototype.changeWatchedEntry = function(entry) {
  if (this.watchedDirectoryEntry_) {
    chrome.fileBrowserPrivate.removeFileWatch(
        this.watchedDirectoryEntry_.toURL(),
        function(result) {
          if (!result) {
            console.log('Failed to remove file watch');
          }
        });
  }
  this.watchedDirectoryEntry_ = entry;

  if (this.watchedDirectoryEntry_) {
    chrome.fileBrowserPrivate.addFileWatch(
        this.watchedDirectoryEntry_.toURL(),
        function(result) {
          if (!result) {
            console.log('Failed to add file watch');
            if (this.watchedDirectoryEntry_ == entry)
              this.watchedDirectoryEntry_ = null;
          }
        }.bind(this));
  }
};

/**
 * @return {DirectoryEntry} Current watched directory entry.
 */
FileWatcher.prototype.getWatchedDirectoryEntry = function() {
  return this.watchedDirectoryEntry_;
};
