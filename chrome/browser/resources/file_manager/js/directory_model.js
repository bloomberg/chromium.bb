// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// If directory files changes too often, don't rescan directory more than once
// per specified interval
var SIMULTANEOUS_RESCAN_INTERVAL = 1000;
// Used for operations that require almost instant rescan.
var SHORT_RESCAN_INTERVAL = 100;

/**
 * Data model of the file manager.
 *
 * @param {DirectoryEntry} root File system root.
 * @param {boolean} singleSelection True if only one file could be selected
 *                                  at the time.
 * @param {MetadataCache} metadataCache The metadata cache service.
 * @param {VolumeManager} volumeManager The volume manager.
 * @param {boolean} isDriveEnabled True if DRIVE enabled (initial value).
 * @constructor
 */
function DirectoryModel(root, singleSelection,
                        metadataCache, volumeManager, isDriveEnabled) {
  this.root_ = root;
  var fileList = new cr.ui.ArrayDataModel([]);
  this.fileListSelection_ = singleSelection ?
      new cr.ui.ListSingleSelectionModel() : new cr.ui.ListSelectionModel();

  this.runningScan_ = null;
  this.pendingScan_ = null;
  this.rescanTime_ = null;
  this.scanFailures_ = 0;
  this.driveEnabled_ = isDriveEnabled;

  this.currentFileListContext_ = new FileListContext(
      metadataCache, fileList, false);
  this.currentDirContents_ = new DirectoryContentsBasic(
      this.currentFileListContext_, root);

  this.rootsList_ = new cr.ui.ArrayDataModel([]);

  this.volumeManager_ = volumeManager;
}

/**
 * Fake entry to be used in currentDirEntry_ when current directory is
 * unmounted DRIVE. TODO(haruki): Support "drive/root" and "drive/other".
 * @type {Object}
 * @const
 * @private
 */
DirectoryModel.fakeDriveEntry_ = {
  fullPath: RootDirectory.DRIVE,
  isDirectory: true
};

/**
 * Fake entry representing a psuedo directory, which contains Drive files
 * available offline. This entry works as a trigger to start a search using
 * DirectoryContentsDriveOffline. specialSearch() must be called to start a
 * special search.
 * @type {Object}
 * @const
 * @private
 */
DirectoryModel.fakeDriveOfflineEntry_ = {
  fullPath: RootDirectory.DRIVE_OFFLINE,
  isDirectory: true
};

/**
 * Fake entry representing a psuedo directory, which contains shared-with-me
 * Drive files. This entry works as a trigger to start a search for
 * shared-with-me files.
 * @type {Object}
 * @const
 * @private
 */
DirectoryModel.fakeDriveSharedWithMeEntry_ = {
  fullPath: RootDirectory.DRIVE_SHARED_WITH_ME,
  isDirectory: true
};

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
  this.volumeManager_.addEventListener('drive-status-changed',
      this.onDriveStatusChanged_.bind(this));
  this.updateRoots_();
};

/**
 * @return {cr.ui.ArrayDataModel} Files in the current directory.
 */
DirectoryModel.prototype.getFileList = function() {
  return this.currentFileListContext_.fileList;
};

/**
 * @return {MetadataCache} Metadata cache.
 */
DirectoryModel.prototype.getMetadataCache = function() {
  return this.currentFileListContext_.metadataCache;
};

/**
 * Sets whether DRIVE appears in the roots list and
 * if it could be used as current directory.
 * @param {boolead} enabled True if DRIVE enabled.
 */
DirectoryModel.prototype.setDriveEnabled = function(enabled) {
  if (this.driveEnabled_ == enabled)
    return;
  this.driveEnabled_ = enabled;
  this.updateRoots_();
  if (!enabled && (this.getCurrentRootType() == RootType.DRIVE ||
                   this.getCurrentRootType() == RootType.DRIVE_OFFLINE))
    this.changeDirectory(this.getDefaultDirectory());
};

/**
 * Sort the file list.
 * @param {string} sortField Sort field.
 * @param {string} sortDirection "asc" or "desc".
 */
DirectoryModel.prototype.sortFileList = function(sortField, sortDirection) {
  this.getFileList().sort(sortField, sortDirection);
};

/**
 * @return {cr.ui.ListSelectionModel|cr.ui.ListSingleSelectionModel} Selection
 * in the fileList.
 */
DirectoryModel.prototype.getFileListSelection = function() {
  return this.fileListSelection_;
};

/**
 * @return {RootType} Root type of current root.
 */
DirectoryModel.prototype.getCurrentRootType = function() {
  return PathUtil.getRootType(this.currentDirContents_.getPath());
};

/**
 * @return {string} Root name.
 */
DirectoryModel.prototype.getCurrentRootName = function() {
  var rootPath = PathUtil.split(this.getCurrentRootPath());
  return rootPath[rootPath.length - 1];
};

/**
 * @return {string} Root name.
 */
DirectoryModel.prototype.getCurrentRootPath = function() {
  return PathUtil.getRootPath(this.currentDirContents_.getPath());
};

/**
 * @return {string} Root name.
 */
DirectoryModel.prototype.getCurrentRootUrl = function() {
  return util.makeFilesystemUrl(this.getCurrentRootPath());
};

/**
 * @return {boolean} on True if offline.
 */
DirectoryModel.prototype.isDriveOffline = function() {
  var connection = this.volumeManager_.getDriveConnectionState();
  return connection.type == VolumeManager.DriveConnectionType.OFFLINE;
};

/**
 * TODO(haruki): This actually checks the current root. Fix the method name and
 * related code.
 * @return {boolean} True if the root for the current directory is read only.
 */
DirectoryModel.prototype.isReadOnly = function() {
  return this.isPathReadOnly(this.getCurrentRootPath());
};

/**
 * @return {boolean} True if the a scan is active.
 */
DirectoryModel.prototype.isScanning = function() {
  return this.currentDirContents_.isScanning();
};

/**
 * @return {boolean} True if search is in progress.
 */
DirectoryModel.prototype.isSearching = function() {
  return this.currentDirContents_.isSearch();
};

/**
 * @param {string} path Path to check.
 * @return {boolean} True if the |path| is read only.
 */
DirectoryModel.prototype.isPathReadOnly = function(path) {
  switch (PathUtil.getRootType(path)) {
    case RootType.REMOVABLE:
      return !!this.volumeManager_.isReadOnly(PathUtil.getRootPath(path)) ||
             !!this.volumeManager_.getMountError(PathUtil.getRootPath(path));
    case RootType.ARCHIVE:
      return true;
    case RootType.DOWNLOADS:
      return false;
    case RootType.DRIVE:
      // TODO(haruki): Maybe add DRIVE_OFFLINE as well to allow renaming in the
      // offline tab.
      return this.isDriveOffline();
    default:
      return true;
  }
};

/**
 * @return {boolean} If the files with names starting with "." are not shown.
 */
DirectoryModel.prototype.isFilterHiddenOn = function() {
  return this.currentFileListContext_.isFilterHiddenOn();
};

/**
 * @param {boolean} value Whether files with leading "." are hidden.
 */
DirectoryModel.prototype.setFilterHidden = function(value) {
  this.currentFileListContext_.setFilterHidden(value);
  this.rescanSoon();
};

/**
 * @return {DirectoryEntry} Current directory.
 */
DirectoryModel.prototype.getCurrentDirEntry = function() {
  return this.currentDirContents_.getDirectoryEntry();
};

/**
 * @return {string} URL of the current directory. or null if unavailable.
 */
DirectoryModel.prototype.getCurrentDirectoryURL = function() {
  var entry = this.currentDirContents_.getDirectoryEntry();
  if (entry == DirectoryModel.fakeDriveOfflineEntry_) {
    return util.makeFilesystemUrl(entry.fullPath);
  }
  return entry.toURL();
};

/**
 * @return {string} Path for the current directory.
 */
DirectoryModel.prototype.getCurrentDirPath = function() {
  return this.currentDirContents_.getPath();
};

/**
 * @return {Array.<string>} File paths of selected files.
 * @private
 */
DirectoryModel.prototype.getSelectedPaths_ = function() {
  var indexes = this.fileListSelection_.selectedIndexes;
  var fileList = this.getFileList();
  if (fileList) {
    return indexes.map(function(i) {
      return fileList.item(i).fullPath;
    });
  }
  return [];
};

/**
 * @param {Array.<string>} value List of file paths of selected files.
 * @private
 */
DirectoryModel.prototype.setSelectedPaths_ = function(value) {
  var indexes = [];
  var fileList = this.getFileList();

  var safeKey = function(key) {
    // The transformation must:
    // 1. Never generate a reserved name ('__proto__')
    // 2. Keep different keys different.
    return '#' + key;
  };

  var hash = {};

  for (var i = 0; i < value.length; i++)
    hash[safeKey(value[i])] = 1;

  for (var i = 0; i < fileList.length; i++) {
    if (hash.hasOwnProperty(safeKey(fileList.item(i).fullPath)))
      indexes.push(i);
  }
  this.fileListSelection_.selectedIndexes = indexes;
};

/**
 * @return {string} Lead item file path.
 * @private
 */
DirectoryModel.prototype.getLeadPath_ = function() {
  var index = this.fileListSelection_.leadIndex;
  return index >= 0 && this.getFileList().item(index).fullPath;
};

/**
 * @param {string} value The name of new lead index.
 * @private
 */
DirectoryModel.prototype.setLeadPath_ = function(value) {
  var fileList = this.getFileList();
  for (var i = 0; i < fileList.length; i++) {
    if (fileList.item(i).fullPath === value) {
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
  if (this.rescanTime_) {
    if (this.rescanTime_ <= Date.now() + delay)
      return;
    clearTimeout(this.rescanTimeoutId_);
  }

  this.rescanTime_ = Date.now() + delay;
  this.rescanTimeoutId_ = setTimeout(this.rescan.bind(this), delay);
};

/**
 * Cancel a rescan on timeout if it is scheduled.
 * @private
 */
DirectoryModel.prototype.clearRescanTimeout_ = function() {
  this.rescanTime_ = null;
  if (this.rescanTimeoutId_) {
    clearTimeout(this.rescanTimeoutId_);
    this.rescanTimeoutId_ = null;
  }
};

/**
 * Rescan current directory. May be called indirectly through rescanLater or
 * directly in order to reflect user action. Will first cache all the directory
 * contents in an array, then seamlessly substitute the fileList contents,
 * preserving the select element etc.
 *
 * This should be to scan the contents of current directory (or search).
 */
DirectoryModel.prototype.rescan = function() {
  this.clearRescanTimeout_();
  if (this.runningScan_) {
    this.pendingRescan_ = true;
    return;
  }

  var dirContents = this.currentDirContents_.clone();
  dirContents.setFileList([]);

  var successCallback = (function() {
    this.replaceDirectoryContents_(dirContents);
    cr.dispatchSimpleEvent(this, 'rescan-completed');
  }).bind(this);

  this.scan_(dirContents, successCallback);
};

/**
 * Run scan on the current DirectoryContents. The active fileList is cleared and
 * the entries are added directly.
 *
 * This should be used when changing directory or initiating a new search.
 *
 * @param {DirectoryContentes} newDirContents New DirectoryContents instance to
 *     replace currentDirContents_.
 * @param {function=} opt_callback Called on success.
 * @private
 */
DirectoryModel.prototype.clearAndScan_ = function(newDirContents,
                                                  opt_callback) {
  if (this.currentDirContents_.isScanning())
    this.currentDirContents_.cancelScan();
  this.currentDirContents_ = newDirContents;
  this.clearRescanTimeout_();

  if (this.pendingScan_)
    this.pendingScan_ = false;

  if (this.runningScan_) {
    this.runningScan_.cancelScan();
    this.runningScan_ = null;
  }

  var onDone = function() {
    cr.dispatchSimpleEvent(this, 'scan-completed');
    if (opt_callback)
      opt_callback();
  }.bind(this);

  // Clear the table first.
  var fileList = this.getFileList();
  fileList.splice(0, fileList.length);
  cr.dispatchSimpleEvent(this, 'scan-started');
  this.scan_(this.currentDirContents_, onDone);
};

/**
 * Perform a directory contents scan. Should be called only from rescan() and
 * clearAndScan_().
 *
 * @param {DirectoryContents} dirContents DirectoryContents instance on which
 *     the scan will be run.
 * @param {function} successCallback Callback on success.
 * @private
 */
DirectoryModel.prototype.scan_ = function(dirContents, successCallback) {
  var self = this;

  /**
   * Runs pending scan if there is one.
   *
   * @return {boolean} Did pending scan exist.
   */
  var maybeRunPendingRescan = function() {
    if (self.pendingRescan_) {
      self.rescanSoon();
      self.pendingRescan_ = false;
      return true;
    }
    return false;
  };

  var onSuccess = function() {
    self.runningScan_ = null;
    successCallback();
    self.scanFailures_ = 0;
    maybeRunPendingRescan();
  };

  var onFailure = function() {
    self.runningScan_ = null;
    self.scanFailures_++;

    if (maybeRunPendingRescan())
      return;

    if (self.scanFailures_ <= 1)
      self.rescanLater();
  };

  this.runningScan_ = dirContents;

  dirContents.addEventListener('scan-completed', onSuccess);
  dirContents.addEventListener('scan-failed', onFailure);
  dirContents.addEventListener('scan-cancelled', this.dispatchEvent.bind(this));
  dirContents.scan();
};

/**
 * @param {DirectoryContents} dirContents DirectoryContents instance.
 * @private
 */
DirectoryModel.prototype.replaceDirectoryContents_ = function(dirContents) {
  cr.dispatchSimpleEvent(this, 'begin-update-files');
  this.fileListSelection_.beginChange();

  var selectedPaths = this.getSelectedPaths_();
  var selectedIndices = this.fileListSelection_.selectedIndexes;

  // Restore leadIndex in case leadName no longer exists.
  var leadIndex = this.fileListSelection_.leadIndex;
  var leadPath = this.getLeadPath_();

  this.currentDirContents_ = dirContents;
  dirContents.replaceContextFileList();

  this.setSelectedPaths_(selectedPaths);
  this.fileListSelection_.leadIndex = leadIndex;
  this.setLeadPath_(leadPath);

  // If nothing is selected after update, then select file next to the
  // latest selection
  if (this.fileListSelection_.selectedIndexes.length == 0 &&
      selectedIndices.length != 0) {
    var maxIdx = Math.max.apply(null, selectedIndices);
    this.selectIndex(Math.min(maxIdx - selectedIndices.length + 2,
                              this.getFileList().length) - 1);
  }
  this.fileListSelection_.endChange();

  cr.dispatchSimpleEvent(this, 'end-update-files');
};

/**
 * @param {string} name Filename.
 */
DirectoryModel.prototype.onEntryChanged = function(name) {
  var currentEntry = this.getCurrentDirEntry();
  var fileList = this.getFileList();
  var self = this;

  var onEntryFound = function(entry) {
    // Do nothing if current directory changed during async operations.
    if (self.getCurrentDirEntry() != currentEntry)
      return;
    self.currentDirContents_.prefetchMetadata([entry], function() {
      // Do nothing if current directory changed during async operations.
      if (self.getCurrentDirEntry() != currentEntry)
        return;

      var index = self.findIndexByName_(name);
      if (index >= 0)
        fileList.splice(index, 1, entry);
      else
        fileList.splice(fileList.length, 0, entry);
    });
  };

  var onError = function(err) {
    if (err.code != FileError.NOT_FOUND_ERR) {
      self.rescanLater();
      return;
    }

    var index = self.findIndexByName_(name);
    if (index >= 0)
      fileList.splice(index, 1);
  };

  util.resolvePath(currentEntry, name, onEntryFound, onError);
};

/**
 * @param {string} name Filename.
 * @return {number} The index in the fileList.
 * @private
 */
DirectoryModel.prototype.findIndexByName_ = function(name) {
  var fileList = this.getFileList();
  for (var i = 0; i < fileList.length; i++)
    if (fileList.item(i).name == name)
      return i;
  return -1;
};

/**
 * Rename the entry in the filesystem and update the file list.
 * @param {Entry} entry Entry to rename.
 * @param {string} newName New name.
 * @param {function} errorCallback Called on error.
 * @param {function=} opt_successCallback Called on success.
 */
DirectoryModel.prototype.renameEntry = function(entry, newName,
                                                errorCallback,
                                                opt_successCallback) {
  var currentDirPath = this.getCurrentDirPath();
  var onSuccess = function(newEntry) {
    this.currentDirContents_.prefetchMetadata([newEntry], function() {
      // Do not change anything or call the callback if current
      // directory changed.
      if (currentDirPath != this.getCurrentDirPath())
        return;

      var index = this.findIndexByName_(entry.name);

      if (index >= 0) {
        var wasSelected = this.fileListSelection_.getIndexSelected(index);

        this.getFileList().splice(index, 1, newEntry);

        if (wasSelected)
          this.fileListSelection_.setIndexSelected(
              this.findIndexByName_(newName), true);
      }

      // If the entry doesn't exist in the list it mean that it updated from
      // outside (probably by directory rescan).
      if (opt_successCallback)
         opt_successCallback();
    }.bind(this));
  }.bind(this);

  var onParentFound = function(parentEntry) {
    entry.moveTo(parentEntry, newName, onSuccess, errorCallback);
  };

  entry.getParent(onParentFound, errorCallback);
};

/**
 * Checks if current directory contains a file or directory with this name.
 * @param {string} entry Entry to which newName will be given.
 * @param {string} name Name to check.
 * @param {function(boolean, boolean?)} callback Called when the result's
 *     available. First parameter is true if the entry exists and second
 *     is true if it's a file.
 */
DirectoryModel.prototype.doesExist = function(entry, name, callback) {
  var onParentFound = function(parentEntry) {
    util.resolvePath(parentEntry, name,
        function(foundEntry) {
          callback(true, foundEntry.isFile);
        },
        callback.bind(window, false));
  };

  entry.getParent(onParentFound, callback.bind(window, false));
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
  var currentDirPath = this.getCurrentDirPath();

  var onSuccess = function(newEntry) {
    // Do not change anything or call the callback if current
    // directory changed.
    if (currentDirPath != this.getCurrentDirPath())
      return;

    var existing = this.getFileList().slice().filter(
        function(e) {return e.name == name;});

    if (existing.length) {
      this.selectEntry(name);
      successCallback(existing[0]);
    } else {
      this.fileListSelection_.beginChange();
      this.getFileList().splice(0, 0, newEntry);
      this.selectEntry(name);
      this.fileListSelection_.endChange();
      successCallback(newEntry);
    }
  };

  this.currentDirContents_.createDirectory(name, onSuccess.bind(this),
                                           errorCallback);
};

/**
 * Changes directory. Causes 'directory-change' event.
 *
 * @param {string} path New current directory path.
 * @param {function()=} opt_errorCallback Executed if the change directory
 * failed.
 */
DirectoryModel.prototype.changeDirectory = function(path, opt_errorCallback) {
  if (PathUtil.isSpecialSearchRoot(path)) {
    this.specialSearch(path, '');
    return;
  }

  this.resolveDirectory(path, function(directoryEntry) {
    this.changeDirectoryEntry_(false, directoryEntry);
  }.bind(this), function(error) {
    console.error('Error changing directory to ' + path + ': ', error);
    if (opt_errorCallback)
      opt_errorCallback();
  });
};

/**
 * Resolves absolute directory path. Handles Drive stub.
 * @param {string} path Path to the directory.
 * @param {function(DirectoryEntry} successCallback Success callback.
 * @param {function(FileError} errorCallback Error callback.
 */
DirectoryModel.prototype.resolveDirectory = function(path, successCallback,
                                                     errorCallback) {
  if (PathUtil.getRootType(path) == RootType.DRIVE) {
    if (!this.isDriveMounted()) {
      if (path == DirectoryModel.fakeDriveEntry_.fullPath)
        successCallback(DirectoryModel.fakeDriveEntry_);
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
 * @param {DirectoryEntry} dirEntry The absolute path to the new directory.
 * @param {function=} opt_callback Executed if the directory loads successfully.
 * @private
 */
DirectoryModel.prototype.changeDirectoryEntrySilent_ = function(dirEntry,
                                                                opt_callback) {
  var onScanComplete = function() {
    if (opt_callback)
      opt_callback();
    // For tests that open the dialog to empty directories, everything
    // is loaded at this point.
    chrome.test.sendMessage('directory-change-complete');
  };
  this.clearAndScan_(new DirectoryContentsBasic(this.currentFileListContext_,
                                                dirEntry),
                     onScanComplete.bind(this));
};

/**
 * Change the current directory to the directory represented by a
 * DirectoryEntry.
 *
 * Dispatches the 'directory-changed' event when the directory is successfully
 * changed.
 *
 * @param {boolean} initial True if it comes from setupPath and
 *                          false if caused by an user action.
 * @param {DirectoryEntry} dirEntry The absolute path to the new directory.
 * @param {function=} opt_callback Executed if the directory loads successfully.
 * @private
 */
DirectoryModel.prototype.changeDirectoryEntry_ = function(initial, dirEntry,
                                                          opt_callback) {
  if (dirEntry == DirectoryModel.fakeDriveEntry_ &&
      this.volumeManager_.getDriveStatus() ==
          VolumeManager.DriveStatus.UNMOUNTED) {
    this.volumeManager_.mountDrive(function() {}, function() {});
  }

  var previous = this.currentDirContents_.getDirectoryEntry();
  this.clearSearch_();
  this.changeDirectoryEntrySilent_(dirEntry, opt_callback);

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
        this.active_ = false;
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
 * @param {string} path The root path to use.
 * @param {function=} opt_pathResolveCallback Invoked as soon as the path has
 *     been resolved, and called with the base and leaf portions of the path
 *     name, and a flag indicating if the entry exists. Will be called even
 *     if another directory change happened while setupPath was in progress,
 *     but will pass |false| as |exist| parameter.
 */
DirectoryModel.prototype.setupPath = function(path, opt_pathResolveCallback) {
  var tracker = this.createDirectoryChangeTracker();
  tracker.start();

  var self = this;
  var resolveCallback = function(directoryPath, fileName, exists) {
    tracker.stop();
    if (!opt_pathResolveCallback)
      return;
    opt_pathResolveCallback(directoryPath, fileName,
                            exists && !tracker.hasChanged);
  };

  var changeDirectoryEntry = function(directoryEntry, initial, opt_callback) {
    tracker.stop();
    if (!tracker.hasChanged)
      self.changeDirectoryEntry_(initial, directoryEntry, opt_callback);
  };

  var INITIAL = true;
  var EXISTS = true;

  var changeToDefault = function(leafName) {
    var def = self.getDefaultDirectory();
    self.resolveDirectory(def, function(directoryEntry) {
      resolveCallback(def, leafName, !EXISTS);
      changeDirectoryEntry(directoryEntry, INITIAL);
    }, function(error) {
      console.error('Failed to resolve default directory: ' + def, error);
      resolveCallback('/', leafName, !EXISTS);
    });
  };

  var noParentDirectory = function(leafName, error) {
    console.warn('Can\'t resolve parent directory: ' + path, error);
    changeToDefault(leafName);
  };

  if (DirectoryModel.isSystemDirectory(path)) {
    changeToDefault('');
    return;
  }

  this.resolveDirectory(path, function(directoryEntry) {
    resolveCallback(directoryEntry.fullPath, '', !EXISTS);
    changeDirectoryEntry(directoryEntry, INITIAL);
  }, function(error) {
    // Usually, leaf does not exist, because it's just a suggested file name.
    var fileExists = error.code == FileError.TYPE_MISMATCH_ERR;
    var nameDelimiter = path.lastIndexOf('/');
    var parentDirectoryPath = path.substr(0, nameDelimiter);
    var leafName = path.substr(nameDelimiter + 1);
    if (fileExists || error.code == FileError.NOT_FOUND_ERR) {
      if (DirectoryModel.isSystemDirectory(parentDirectoryPath)) {
        changeToDefault(leafName);
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
                             });
        // TODO(kaznacheev): Fix history.replaceState for the File Browser and
        // change !INITIAL to INITIAL. Passing |false| makes things
        // less ugly for now.
      }, noParentDirectory.bind(null, leafName));
    } else {
      // Unexpected errors.
      console.error('Directory resolving error: ', error);
      changeToDefault(leafName);
    }
  });
};

/**
 * Sets up the default path.
 */
DirectoryModel.prototype.setupDefaultPath = function() {
  this.setupPath(this.getDefaultDirectory());
};

/**
 * @return {string} The default directory.
 */
DirectoryModel.prototype.getDefaultDirectory = function() {
  return RootDirectory.DOWNLOADS;
};

/**
 * @param {string} name Filename.
 */
DirectoryModel.prototype.selectEntry = function(name) {
  var fileList = this.getFileList();
  for (var i = 0; i < fileList.length; i++) {
    if (fileList.item(i).name == name) {
      this.selectIndex(i);
      return;
    }
  }
};

/**
 * @param {Array.<string>} urls Array of URLs.
 */
DirectoryModel.prototype.selectUrls = function(urls) {
  var fileList = this.getFileList();
  this.fileListSelection_.beginChange();
  this.fileListSelection_.unselectAll();
  for (var i = 0; i < fileList.length; i++) {
    if (urls.indexOf(fileList.item(i).toURL()) >= 0)
      this.fileListSelection_.setIndexSelected(i, true);
  }
  this.fileListSelection_.endChange();
};

/**
 * @param {number} index Index of file.
 */
DirectoryModel.prototype.selectIndex = function(index) {
  // this.focusCurrentList_();
  if (index >= this.getFileList().length)
    return;

  // If a list bound with the model it will do scrollIndexIntoView(index).
  this.fileListSelection_.selectedIndex = index;
};

/**
 * Get root entries asynchronously.
 * @param {function(Array.<Entry>)} callback Called when roots are resolved.
 * @private
 */
DirectoryModel.prototype.resolveRoots_ = function(callback) {
  var groups = {
    downloads: null,
    archives: null,
    removables: null,
    drive: null,
    driveFakeRoots: null
  };
  var self = this;

  metrics.startInterval('Load.Roots');
  var done = function() {
    for (var i in groups)
      if (!groups[i])
        return;

    callback(groups.downloads.
             concat(groups.drive).
             concat(groups.driveFakeRoots).
             concat(groups.archives).
             concat(groups.removables));
    metrics.recordInterval('Load.Roots');
  };

  var append = function(index, values, opt_error) {
    groups[index] = values;
    done();
  };

  var appendSingle = function(index, entry) {
    groups[index] = [entry];
    done();
  };

  var onSingleError = function(index, defaultValue, error) {
    groups[index] = defaultValue || [];
    done();
    console.error('Error resolving root dir ', index, 'error: ', error);
  };

  var root = this.root_;
  var readSingle = function(dir, index, opt_defaultValue) {
    root.getDirectory(dir, { create: false },
                      appendSingle.bind(this, index),
                      onSingleError.bind(this, index, opt_defaultValue));
  };

  readSingle(RootDirectory.DOWNLOADS.substring(1), 'downloads');
  util.readDirectory(root, RootDirectory.ARCHIVE.substring(1),
                     append.bind(this, 'archives'));
  util.readDirectory(root, RootDirectory.REMOVABLE.substring(1),
                     append.bind(this, 'removables'));

  if (this.driveEnabled_) {
    // TODO(haruki): Add fake special search entries.
    // http://crbug.com/168742 http://crbug.com/168741 http://crbug.com/168739.
    groups.driveFakeRoots = [];
    var fake = [DirectoryModel.fakeDriveEntry_];
    if (this.isDriveMounted()) {
      readSingle(DirectoryModel.fakeDriveEntry_.fullPath, 'drive', fake);
    } else {
      groups.drive = fake;
      done();
    }
  } else {
    groups.driveFakeRoots = [];
    groups.drive = [];
    done();
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
  });
};

/**
 * @return {boolean} True if DRIVE is fully mounted.
 */
DirectoryModel.prototype.isDriveMounted = function() {
  return this.volumeManager_.getDriveStatus() ==
      VolumeManager.DriveStatus.MOUNTED;
};

/**
 * Handler for the VolumeManager's 'change' event.
 * @private
 */
DirectoryModel.prototype.onMountChanged_ = function() {
  this.updateRoots_();

  var rootType = this.getCurrentRootType();

  if ((rootType == RootType.ARCHIVE || rootType == RootType.REMOVABLE) &&
      !this.volumeManager_.isMounted(this.getCurrentRootPath())) {
    this.changeDirectory(this.getDefaultDirectory());
  }
};

/**
 * Handler for the VolumeManager's 'drive-status-changed' event.
 * @private
 */
DirectoryModel.prototype.onDriveStatusChanged_ = function() {
  var mounted = this.isDriveMounted();
  if (mounted) {
    // Change fake entry to real one and rescan.
    var onGotDirectory = function(entry) {
      this.updateRootEntry_(entry);
      var currentDirEntry = this.getCurrentDirEntry();
      if (currentDirEntry == DirectoryModel.fakeDriveEntry_) {
        this.changeDirectoryEntrySilent_(entry);
      } else if (PathUtil.isSpecialSearchRoot(currentDirEntry.fullPath)) {
        this.rescan();
      }
    }
    this.root_.getDirectory(
        DirectoryModel.fakeDriveEntry_.fullPath, {},
        onGotDirectory.bind(this));
  } else {
    var rootType = this.getCurrentRootType();
    if (rootType != RootType.DRIVE && rootType != RootType.DRIVE_OFFLINE)
      return;

    // Current entry unmounted. Replace with fake drive one.
    this.updateRootEntry_(DirectoryModel.fakeDriveEntry_);
    if (this.getCurrentDirPath() == DirectoryModel.fakeDriveEntry_.fullPath) {
      // Replace silently and rescan.
      this.changeDirectoryEntrySilent_(DirectoryModel.fakeDriveEntry_);
    } else {
      this.changeDirectoryEntry_(false, DirectoryModel.fakeDriveEntry_);
    }
  }
};

/**
 * Update the entry in the roots list model.
 *
 * @param {DirectoryEntry} entry New entry.
 * @private
 */
DirectoryModel.prototype.updateRootEntry_ = function(entry) {
  for (var i = 0; i != this.rootsList_.length; i++) {
    if (this.rootsList_.item(i).fullPath == entry.fullPath) {
      this.rootsList_.splice(i, 1, entry);
      return;
    }
  }
  console.error('Cannot find root: ' + entry.fullPath);
};

/**
 * Finds the first entry in the roots list model which has the given root type.
 *
 * @param {RootType} rootType RootType of the desired root..
 * @return {DirectoryEntry} Root entry for with the given root type.
 * @private
 */
DirectoryModel.prototype.findRootEntryByType_ = function(rootType) {
  var root;
  for (var index = 0; index < this.rootsList_.length; index++) {
    if (PathUtil.getRootType(this.rootsList_.item(index).fullPath) ==
        rootType) {
      root = this.rootsList_.item(index);
      break;
    }
  }
  return root;
};

/**
 * @param {string} path Path.
 * @return {boolean} If current directory is system.
 */
DirectoryModel.isSystemDirectory = function(path) {
  path = path.replace(/\/+$/, '');
  return path === RootDirectory.REMOVABLE || path === RootDirectory.ARCHIVE;
};

/**
 * Check if the root of the given path is mountable or not.
 *
 * @param {string} path Path.
 * @return {boolean} Return true, if the given path is under mountable root.
 *     Otherwise, return false.
 */
DirectoryModel.isMountableRoot = function(path) {
  var rootType = PathUtil.getRootType(path);
  switch (rootType) {
    case RootType.DOWNLOADS:
      return false;
    case RootType.ARCHIVE:
    case RootType.REMOVABLE:
    case RootType.DRIVE:
      return true;
    default:
      throw new Error('Unknown root type!');
  }
};

/**
 * Performs search and displays results. The search type is dependent on the
 * current directory. If we are currently on drive, server side content search
 * over drive mount point. If the current directory is not on the drive, file
 * name search over current directory wil be performed.
 *
 * @param {string} query Query that will be searched for.
 * @param {function} onSearchRescan Function that will be called when the search
 *     directory is rescanned (i.e. search results are displayed).
 * @param {function} onClearSearch Function to be called when search state gets
 *     cleared.
 * TODO(olege): Change callbacks to events.
 */
DirectoryModel.prototype.search = function(query,
                                           onSearchRescan,
                                           onClearSearch) {
  if (PathUtil.isSpecialSearchRoot(this.getCurrentDirPath())) {
    this.specialSearch(this.getCurrentDirPath(), query);
    return;
  }

  query = query.trimLeft();

  this.clearSearch_();

  var newDirContents;
  if (!query) {
    if (this.isSearching()) {
      newDirContents = new DirectoryContentsBasic(
          this.currentFileListContext_,
          this.currentDirContents_.getLastNonSearchDirectoryEntry());
      this.clearAndScan_(newDirContents);
    }
    return;
  }

  this.onSearchCompleted_ = onSearchRescan;
  this.onClearSearch_ = onClearSearch;

  this.addEventListener('scan-completed', this.onSearchCompleted_);

  // If we are offline, let's fallback to file name search inside dir.
  if (this.getCurrentRootType() === RootType.DRIVE && !this.isDriveOffline()) {
    // Drive search is performed over the whole drive, so pass drive root as
    // |directoryEntry|.
    newDirContents = new DirectoryContentsDriveSearch(
        this.currentFileListContext_,
        this.getCurrentDirEntry(),
        this.currentDirContents_.getLastNonSearchDirectoryEntry(),
        query,
        DirectoryContentsDriveSearch.SearchType.SEARCH_FULL);
  } else {
    newDirContents = new DirectoryContentsLocalSearch(
        this.currentFileListContext_, this.getCurrentDirEntry(), query);
  }
  this.clearAndScan_(newDirContents);
};

/**
 * Performs special search and displays results. e.g. Drive files available
 * offline, shared-with-me files, recently modified files.
 * @param {string} path Path string representing special search. See fake
 *     entries in PathUtil.RootDirectory.
 * @param {string=} opt_query Query string used for the search.
 */
DirectoryModel.prototype.specialSearch = function(path, opt_query) {
  // Any special search needs Drive mounted.
  if (this.volumeManager_.getDriveStatus() ==
        VolumeManager.DriveStatus.UNMOUNTED) {
    this.volumeManager_.mountDrive(function() {}, function() {});
  }

  var query = opt_query || '';

  this.clearSearch_();

  this.onSearchCompleted_ = null;
  this.onClearSearch_ = null;

  var driveRoot = this.findRootEntryByType_(RootType.DRIVE);
  if (!driveRoot || driveRoot == DirectoryModel.fakeDriveEntry_) {
    // Drive root not available or not ready. onDriveStatusChanged_() handles
    // rescan if necessary.
    driveRoot = null;
  }

  var specialSearchType = PathUtil.getRootType(path);
  var newDirContents;
  var dirEntry;
  if (specialSearchType == RootType.DRIVE_OFFLINE) {
    newDirContents = new DirectoryContentsDriveOffline(
        this.currentFileListContext_,
        driveRoot,
        DirectoryModel.fakeDriveOfflineEntry_,
        query);
    dirEntry = DirectoryModel.fakeDriveOfflineEntry_;
  } else if (specialSearchType == RootType.DRIVE_SHARED_WITH_ME) {
    newDirContents = new DirectoryContentsDriveSearch(
        this.currentFileListContext_,
        driveRoot,
        this.currentDirContents_.getLastNonSearchDirectoryEntry(),
        query,
        DirectoryContentsDriveSearch.SearchType.SEARCH_SHARED_WITH_ME);
    dirEntry = DirectoryModel.fakeDriveSharedWithMeEntry_;
  } else {
    // Unknown path.
    this.changeDirectory(thid.getDefaultDirectory());
    return;
  }

  var previous = this.currentDirContents_.getDirectoryEntry();
  this.clearAndScan_(newDirContents);

  var e = new cr.Event('directory-changed');
  e.previousDirEntry = previous;
  e.newDirEntry = dirEntry;
  e.initial = false;
  this.dispatchEvent(e);
};

/**
 * In case the search was active, remove listeners and send notifications on
 * its canceling.
 * @private
 */
DirectoryModel.prototype.clearSearch_ = function() {
  if (!this.isSearching())
    return;

  if (this.onSearchCompleted_) {
    this.removeEventListener('scan-completed', this.onSearchCompleted_);
    this.onSearchCompleted_ = null;
  }

  if (this.onClearSearch_) {
    this.onClearSearch_();
    this.onClearSearch_ = null;
  }
};

/**
 * @param {string} name Filter identifier.
 * @param {function(Entry)} callback A filter — a function receiving an Entry,
 *     and returning bool.
 */
DirectoryModel.prototype.addFilter = function(name, callback) {
  this.currentFileListContext_.addFilter(name, callback);
};

/**
 * @param {string} name Filter identifier.
 */
DirectoryModel.prototype.removeFilter = function(name) {
  this.currentFileListContext_.removeFilter(name);
};


/**
 * @param {DirectoryEntry} root Root entry.
 * @param {DirectoryModel} directoryModel Model to watch.
 * @param {VolumeManager} volumeManager Manager to watch.
 * @constructor
 */
function FileWatcher(root, directoryModel, volumeManager) {
  this.root_ = root;
  this.dm_ = directoryModel;
  this.vm_ = volumeManager;
  this.watchedDirectoryEntry_ = null;
  this.updateWatchedDirectoryBound_ =
      this.updateWatchedDirectory_.bind(this);
  this.onDirectoryChangedBound_ =
      this.onDirectoryChanged_.bind(this);
}

/**
 * Starts watching.
 */
FileWatcher.prototype.start = function() {
  chrome.fileBrowserPrivate.onDirectoryChanged.addListener(
        this.onDirectoryChangedBound_);

  this.dm_.addEventListener('directory-changed',
      this.updateWatchedDirectoryBound_);
  this.vm_.addEventListener('change',
      this.updateWatchedDirectoryBound_);

  this.updateWatchedDirectory_();
};

/**
 * Stops watching (must be called before page unload).
 */
FileWatcher.prototype.stop = function() {
  chrome.fileBrowserPrivate.onDirectoryChanged.removeListener(
        this.onDirectoryChangedBound_);

  this.dm_.removeEventListener('directory-changed',
      this.updateWatchedDirectoryBound_);
  this.vm_.removeEventListener('change',
      this.updateWatchedDirectoryBound_);

  if (this.watchedDirectoryEntry_)
    this.changeWatchedEntry(null);
};

/**
 * @param {Object} event chrome.fileBrowserPrivate.onDirectoryChanged event.
 * @private
 */
FileWatcher.prototype.onDirectoryChanged_ = function(event) {
  if (event.directoryUrl == this.watchedDirectoryEntry_.toURL())
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
    case RootType.DRIVE:
      if (!this.vm_.isMounted(RootDirectory.DRIVE))
        break;
    case RootType.DOWNLOADS:
    case RootType.REMOVABLE:
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
            console.error('Failed to remove file watch');
          }
        });
  }
  this.watchedDirectoryEntry_ = entry;

  if (this.watchedDirectoryEntry_) {
    chrome.fileBrowserPrivate.addFileWatch(
        this.watchedDirectoryEntry_.toURL(),
        function(result) {
          if (!result) {
            console.error('Failed to add file watch');
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
