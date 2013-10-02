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
 * @param {boolean} singleSelection True if only one file could be selected
 *                                  at the time.
 * @param {FileFilter} fileFilter Instance of FileFilter.
 * @param {FileWatcher} fileWatcher Instance of FileWatcher.
 * @param {MetadataCache} metadataCache The metadata cache service.
 * @param {VolumeManagerWrapper} volumeManager The volume manager.
 * @param {boolean} showSpecialSearchRoots True if special-search roots are
 *     available. They should be hidden for the dialogs to save files.
 * @constructor
 */
function DirectoryModel(singleSelection, fileFilter, fileWatcher,
                        metadataCache, volumeManager,
                        showSpecialSearchRoots) {
  this.fileListSelection_ = singleSelection ?
      new cr.ui.ListSingleSelectionModel() : new cr.ui.ListSelectionModel();

  this.runningScan_ = null;
  this.pendingScan_ = null;
  this.rescanTime_ = null;
  this.scanFailures_ = 0;
  this.showSpecialSearchRoots_ = showSpecialSearchRoots;

  this.fileFilter_ = fileFilter;
  this.fileFilter_.addEventListener('changed',
                                    this.onFilterChanged_.bind(this));

  this.currentFileListContext_ = new FileListContext(
      fileFilter, metadataCache);
  this.currentDirContents_ = new DirectoryContentsBasic(
      this.currentFileListContext_, null);

  this.volumeManager_ = volumeManager;
  this.volumeManager_.volumeInfoList.addEventListener(
      'splice', this.onVolumeInfoListUpdated_.bind(this));

  this.fileWatcher_ = fileWatcher;
  this.fileWatcher_.addEventListener(
      'watcher-directory-changed',
      this.onWatcherDirectoryChanged_.bind(this));
}

/**
 * Fake entry to be used in currentDirEntry_ when current directory is
 * unmounted DRIVE. TODO(haruki): Support "drive/root" and "drive/other".
 * @type {Object}
 * @const
 * @private
 */
DirectoryModel.fakeDriveEntry_ = {
  fullPath: RootDirectory.DRIVE + '/' + DriveSubRootDirectory.ROOT,
  isDirectory: true
};

/**
 * Fake entry representing a psuedo directory, which contains Drive files
 * available offline. This entry works as a trigger to start a search for
 * offline files.
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
 * Fake entry representing a psuedo directory, which contains Drive files
 * accessed recently. This entry works as a trigger to start a metadata search
 * implemented as DirectoryContentsDriveRecent.
 * DirectoryModel is responsible to start the search when the UI tries to open
 * this fake entry (e.g. changeDirectory()).
 * @type {Object}
 * @const
 * @private
 */
DirectoryModel.fakeDriveRecentEntry_ = {
  fullPath: RootDirectory.DRIVE_RECENT,
  isDirectory: true
};

/**
 * List of fake entries for special searches.
 *
 * @type {Array.<Object>}
 * @const
 */
DirectoryModel.FAKE_DRIVE_SPECIAL_SEARCH_ENTRIES = [
  DirectoryModel.fakeDriveSharedWithMeEntry_,
  DirectoryModel.fakeDriveRecentEntry_,
  DirectoryModel.fakeDriveOfflineEntry_
];

/**
 * DirectoryModel extends cr.EventTarget.
 */
DirectoryModel.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Disposes the directory model by removing file watchers.
 */
DirectoryModel.prototype.dispose = function() {
  this.fileWatcher_.dispose();
};

/**
 * @return {cr.ui.ArrayDataModel} Files in the current directory.
 */
DirectoryModel.prototype.getFileList = function() {
  return this.currentFileListContext_.fileList;
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
  var entry = this.currentDirContents_.getDirectoryEntry();
  return PathUtil.getRootType(entry ? entry.fullPath : '');
};

/**
 * @return {string} Root path.
 */
DirectoryModel.prototype.getCurrentRootPath = function() {
  var entry = this.currentDirContents_.getDirectoryEntry();
  return entry ? PathUtil.getRootPath(entry.fullPath) : '';
};

/**
 * @return {string} Filesystem URL representing the mountpoint for the current
 *     contents.
 */
DirectoryModel.prototype.getCurrentMountPointUrl = function() {
  var rootPath = this.getCurrentRootPath();
  // Special search roots are just showing a search results from DRIVE.
  if (PathUtil.getRootType(rootPath) == RootType.DRIVE ||
      PathUtil.isSpecialSearchRoot(rootPath))
    return util.makeFilesystemUrl(RootDirectory.DRIVE);

  return util.makeFilesystemUrl(rootPath);
};

/**
 * @return {boolean} on True if offline.
 */
DirectoryModel.prototype.isDriveOffline = function() {
  var connection = this.volumeManager_.getDriveConnectionState();
  return connection.type == util.DriveConnectionType.OFFLINE;
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
  // TODO(hidehiko): Migrate this into VolumeInfo.
  switch (PathUtil.getRootType(path)) {
    case RootType.REMOVABLE:
      var volumeInfo = this.volumeManager_.getVolumeInfo(
          PathUtil.getRootPath(path));
      // Returns true if the volume is actually read only, or if an error
      // is found during the mounting.
      // TODO(hidehiko): Remove "error" check here, by removing error'ed volume
      // info from VolumeManager.
      return volumeInfo && (volumeInfo.isReadOnly || !!volumeInfo.error);
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
 * Updates the selection by using the updateFunc and publish the change event.
 * If updateFunc returns true, it force to dispatch the change event even if the
 * selection index is not changed.
 *
 * @param {cr.ui.ListSelectionModel|cr.ui.ListSingleSelectionModel} selection
 *     Selection to be updated.
 * @param {function(): boolean} updateFunc Function updating the selection.
 * @private
 */
DirectoryModel.prototype.updateSelectionAndPublishEvent_ =
    function(selection, updateFunc) {
  // Begin change.
  selection.beginChange();

  // If dispatchNeeded is true, we should ensure the change evnet is
  // dispatched.
  var dispatchNeeded = updateFunc();

  // Check if the change event is dispatched in the endChange function
  // or not.
  var eventDispatched = function() { dispatchNeeded = false; };
  selection.addEventListener('change', eventDispatched);
  selection.endChange();
  selection.removeEventListener('change', eventDispatched);

  // If the change evnet have been already dispatched, dispatchNeeded is false.
  if (dispatchNeeded) {
    var event = new Event('change');
    // The selection status (selected or not) is not changed because
    // this event is caused by the change of selected item.
    event.changes = [];
    selection.dispatchEvent(event);
  }
};

/**
 * Invoked when a change in the directory is detected by the watcher.
 * @private
 */
DirectoryModel.prototype.onWatcherDirectoryChanged_ = function() {
  this.rescanSoon();
};

/**
 * Invoked when filters are changed.
 * @private
 */
DirectoryModel.prototype.onFilterChanged_ = function() {
  this.rescanSoon();
};

/**
 * Returns the filter.
 * @return {FileFilter} The file filter.
 */
DirectoryModel.prototype.getFileFilter = function() {
  return this.fileFilter_;
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
  if (!entry)
    return null;
  if (entry === DirectoryModel.fakeDriveOfflineEntry_)
    return util.makeFilesystemUrl(entry.fullPath);
  return entry.toURL();
};

/**
 * @return {string} Path for the current directory, or empty string if the
 *     current directory is not yet set.
 */
DirectoryModel.prototype.getCurrentDirPath = function() {
  var entry = this.currentDirContents_.getDirectoryEntry();
  return entry ? entry.fullPath : '';
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

  this.scan_(dirContents,
             successCallback, function() {}, function() {}, function() {});
};

/**
 * Run scan on the current DirectoryContents. The active fileList is cleared and
 * the entries are added directly.
 *
 * This should be used when changing directory or initiating a new search.
 *
 * @param {DirectoryContentes} newDirContents New DirectoryContents instance to
 *     replace currentDirContents_.
 * @param {function()=} opt_callback Called on success.
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
    if (this.runningScan_.isScanning())
      this.runningScan_.cancelScan();
    this.runningScan_ = null;
  }

  var onDone = function() {
    cr.dispatchSimpleEvent(this, 'scan-completed');
    if (opt_callback)
      opt_callback();
  }.bind(this);

  var onFailed = function() {
    cr.dispatchSimpleEvent(this, 'scan-failed');
  }.bind(this);

  var onUpdated = function() {
    cr.dispatchSimpleEvent(this, 'scan-updated');
  }.bind(this);

  var onCancelled = function() {
    cr.dispatchSimpleEvent(this, 'scan-cancelled');
  }.bind(this);

  // Clear the table, and start scanning.
  cr.dispatchSimpleEvent(this, 'scan-started');
  var fileList = this.getFileList();
  fileList.splice(0, fileList.length);
  this.scan_(this.currentDirContents_,
             onDone, onFailed, onUpdated, onCancelled);
};

/**
 * Perform a directory contents scan. Should be called only from rescan() and
 * clearAndScan_().
 *
 * @param {DirectoryContents} dirContents DirectoryContents instance on which
 *     the scan will be run.
 * @param {function()} successCallback Callback on success.
 * @param {function()} failureCallback Callback on failure.
 * @param {function()} updatedCallback Callback on update. Only on the last
 *     update, {@code successCallback} is called instead of this.
 * @param {function()} cancelledCallback Callback on cancel.
 * @private
 */
DirectoryModel.prototype.scan_ = function(
    dirContents,
    successCallback, failureCallback, updatedCallback, cancelledCallback) {
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
    failureCallback();

    if (maybeRunPendingRescan())
      return;

    if (self.scanFailures_ <= 1)
      self.rescanLater();
  };

  this.runningScan_ = dirContents;

  dirContents.addEventListener('scan-completed', onSuccess);
  dirContents.addEventListener('scan-updated', updatedCallback);
  dirContents.addEventListener('scan-failed', onFailure);
  dirContents.addEventListener('scan-cancelled', cancelledCallback);
  dirContents.scan();
};

/**
 * @param {DirectoryContents} dirContents DirectoryContents instance.
 * @private
 */
DirectoryModel.prototype.replaceDirectoryContents_ = function(dirContents) {
  cr.dispatchSimpleEvent(this, 'begin-update-files');
  this.updateSelectionAndPublishEvent_(this.fileListSelection_, function() {
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
    var forceChangeEvent = false;
    if (this.fileListSelection_.selectedIndexes.length == 0 &&
        selectedIndices.length != 0) {
      var maxIdx = Math.max.apply(null, selectedIndices);
      this.selectIndex(Math.min(maxIdx - selectedIndices.length + 2,
                                this.getFileList().length) - 1);
      forceChangeEvent = true;
    }
    return forceChangeEvent;
  }.bind(this));

  cr.dispatchSimpleEvent(this, 'end-update-files');
};

/**
 * Callback when an entry is changed.
 * @param {util.EntryChangedKind} kind How the entry is changed.
 * @param {Entry} entry The changed entry.
 */
DirectoryModel.prototype.onEntryChanged = function(kind, entry) {
  // TODO(hidehiko): We should update directory model even the search result
  // is shown.
  var rootType = this.getCurrentRootType();
  if ((rootType === RootType.DRIVE ||
       rootType === RootType.DRIVE_SHARED_WITH_ME ||
       rootType === RootType.DRIVE_RECENT ||
       rootType === RootType.DRIVE_OFFLINE) &&
      this.isSearching())
    return;

  if (kind == util.EntryChangedKind.CREATED) {
    entry.getParent(function(parentEntry) {
      if (this.getCurrentDirEntry().fullPath != parentEntry.fullPath) {
        // Do nothing if current directory changed during async operations.
        return;
      }
      this.currentDirContents_.prefetchMetadata([entry], function() {
        if (this.getCurrentDirEntry().fullPath != parentEntry.fullPath) {
          // Do nothing if current directory changed during async operations.
          return;
        }

        var index = this.findIndexByEntry_(entry);
        if (index >= 0)
          this.getFileList().splice(index, 1, entry);
        else
          this.getFileList().push(entry);
      }.bind(this));
    }.bind(this));
  } else {
    // This is the delete event.
    var index = this.findIndexByEntry_(entry);
    if (index >= 0)
      this.getFileList().splice(index, 1);
  }
};

/**
 * @param {Entry} entry The entry to be searched.
 * @return {number} The index in the fileList, or -1 if not found.
 * @private
 */
DirectoryModel.prototype.findIndexByEntry_ = function(entry) {
  var fileList = this.getFileList();
  for (var i = 0; i < fileList.length; i++) {
    if (util.isSameEntry(fileList.item(i), entry))
      return i;
  }
  return -1;
};

/**
 * Called when rename is done successfully.
 * Note: conceptually, DirectoryModel should work without this, because entries
 * can be renamed by other systems anytime and Files.app should reflect it
 * correctly.
 * TODO(hidehiko): investigate more background, and remove this if possible.
 *
 * @param {Entry} oldEntry The old entry.
 * @param {Entry} newEntry The new entry.
 * @param {function()} opt_callback Called on completion.
 */
DirectoryModel.prototype.onRenameEntry = function(
    oldEntry, newEntry, opt_callback) {
  this.currentDirContents_.prefetchMetadata([newEntry], function() {
    // If the current directory is the old entry, then quietly change to the
    // new one.
    if (util.isSameEntry(oldEntry, this.getCurrentDirEntry()))
      this.changeDirectory(newEntry.fullPath);

    // Look for the old entry.
    // If the entry doesn't exist in the list, it has been updated from
    // outside (probably by directory rescan).
    var index = this.findIndexByEntry_(oldEntry);
    if (index >= 0) {
      // Update the content list and selection status.
      var wasSelected = this.fileListSelection_.getIndexSelected(index);
      this.fileListSelection_.beginChange();
      this.fileListSelection_.setIndexSelected(index, false);
      this.getFileList().splice(index, 1, newEntry);
      if (wasSelected) {
        // We re-search the index, because splice may trigger sorting so that
        // index may be stale.
        this.fileListSelection_.setIndexSelected(
            this.findIndexByEntry_(newEntry), true);
      }
      this.fileListSelection_.endChange();
    }

    // Run callback, finally.
    if (opt_callback)
      opt_callback();
  }.bind(this));
};

/**
 * Creates directory and updates the file list.
 *
 * @param {string} name Directory name.
 * @param {function(DirectoryEntry)} successCallback Callback on success.
 * @param {function(FileError)} errorCallback Callback on failure.
 */
DirectoryModel.prototype.createDirectory = function(name, successCallback,
                                                    errorCallback) {
  var entry = this.getCurrentDirEntry();
  if (!entry) {
    errorCallback(util.createFileError(FileError.INVALID_MODIFICATION_ERR));
    return;
  }

  var onSuccess = function(newEntry) {
    // Do not change anything or call the callback if current
    // directory changed.
    if (entry.fullPath != this.getCurrentDirPath())
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
 * @param {function(FileError)=} opt_errorCallback Executed if the change
 *     directory failed.
 */
DirectoryModel.prototype.changeDirectory = function(path, opt_errorCallback) {
  if (PathUtil.isSpecialSearchRoot(path)) {
    this.specialSearch(path, '');
    return;
  }

  this.resolveDirectory(path, function(directoryEntry) {
    this.changeDirectoryEntry_(directoryEntry);
  }.bind(this), function(error) {
    console.error('Error changing directory to ' + path + ': ', error);
    if (opt_errorCallback)
      opt_errorCallback(error);
  });
};

/**
 * Resolves absolute directory path. Handles Drive stub. If the drive is
 * mounting, callbacks will be called after the mount is completed.
 *
 * @param {string} path Path to the directory.
 * @param {function(DirectoryEntry)} successCallback Success callback.
 * @param {function(FileError)} errorCallback Error callback.
 */
DirectoryModel.prototype.resolveDirectory = function(
    path, successCallback, errorCallback) {
  if (PathUtil.getRootType(path) == RootType.DRIVE) {
    if (!this.volumeManager_.getVolumeInfo(RootDirectory.DRIVE)) {
      errorCallback(util.createFileError(FileError.NOT_FOUND_ERR));
      return;
    }
  }

  var onError = function(error) {
    // Handle the special case, when in offline mode, and there are no cached
    // contents on the C++ side. In such case, let's display the stub.
    // The INVALID_STATE_ERR error code is returned from the drive filesystem
    // in such situation.
    //
    // TODO(mtomasz, hashimoto): Consider rewriting this logic.
    //     crbug.com/253464.
    if (PathUtil.getRootType(path) == RootType.DRIVE &&
        error.code == FileError.INVALID_STATE_ERR) {
      successCallback(DirectoryModel.fakeDriveEntry_);
      return;
    }
    errorCallback(error);
  }.bind(this);

  this.volumeManager_.resolvePath(
      path,
      function(entry) {
        if (entry.isFile) {
          onError(util.createFileError(FileError.TYPE_MISMATCH_ERR));
          return;
        }
        successCallback(entry);
      },
      onError);
};

/**
 * @param {DirectoryEntry} dirEntry The absolute path to the new directory.
 * @param {function()=} opt_callback Executed if the directory loads
 *     successfully.
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
 * @param {DirectoryEntry} dirEntry The absolute path to the new directory.
 * @param {function()=} opt_callback Executed if the directory loads
 *     successfully.
 * @private
 */
DirectoryModel.prototype.changeDirectoryEntry_ = function(
    dirEntry, opt_callback) {
  this.fileWatcher_.changeWatchedDirectory(dirEntry, function() {
    var previous = this.currentDirContents_.getDirectoryEntry();
    this.clearSearch_();
    this.changeDirectoryEntrySilent_(dirEntry, opt_callback);

    var e = new cr.Event('directory-changed');
    e.previousDirEntry = previous;
    e.newDirEntry = dirEntry;
    this.dispatchEvent(e);
  }.bind(this));
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
      tracker.stop();
      tracker.hasChanged = true;
    }
  };
  return tracker;
};

/**
 * Change the state of the model to reflect the specified path (either a
 * file or directory).
 * TODO(hidehiko): This logic should be merged with
 * FileManager.setupCurrentDirectory_.
 *
 * @param {string} path The root path to use.
 * @param {function(string, string, boolean)=} opt_pathResolveCallback Invoked
 *     as soon as the path has been resolved, and called with the base and leaf
 *     portions of the path name, and a flag indicating if the entry exists.
 *     Will be called even if another directory change happened while setupPath
 *     was in progress, but will pass |false| as |exist| parameter.
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

  var changeDirectoryEntry = function(directoryEntry, opt_callback) {
    tracker.stop();
    if (!tracker.hasChanged)
      self.changeDirectoryEntry_(directoryEntry, opt_callback);
  };

  var EXISTS = true;

  var changeToDefault = function(leafName) {
    var def = PathUtil.DEFAULT_DIRECTORY;
    self.resolveDirectory(def, function(directoryEntry) {
      resolveCallback(def, leafName, !EXISTS);
      changeDirectoryEntry(directoryEntry);
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
    changeDirectoryEntry(directoryEntry);
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
                             function() {
                               self.selectEntry(fileName);
                             });
      }, noParentDirectory.bind(null, leafName));
    } else {
      // Unexpected errors.
      console.error('Directory resolving error: ', error);
      changeToDefault(leafName);
    }
  });
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
 * Called when VolumeInfoList is updated.
 *
 * @param {cr.Event} event Event of VolumeInfoList's 'sclice'.
 * @private
 */
DirectoryModel.prototype.onVolumeInfoListUpdated_ = function(event) {
  var driveVolume = this.volumeManager_.getVolumeInfo(RootDirectory.DRIVE);
  if (driveVolume && !driveVolume.error) {
    var currentDirEntry = this.getCurrentDirEntry();
    if (currentDirEntry) {
      if (currentDirEntry === DirectoryModel.fakeDriveEntry_) {
        // Replace the fake entry by real DirectoryEntry silently.
        this.volumeManager_.resolvePath(
            DirectoryModel.fakeDriveEntry_.fullPath,
            function(entry) {
              // If the current entry is still fake drive entry, replace it.
              if (this.getCurrentDirEntry() === DirectoryModel.fakeDriveEntry_)
                this.changeDirectoryEntrySilent_(entry);
            },
            function(error) {});
      } else if (PathUtil.isSpecialSearchRoot(currentDirEntry.fullPath)) {
        for (var i = 0; i < event.added.length; i++) {
          if (event.added[i].volumeType == util.VolumeType.DRIVE) {
            // If the Drive volume is newly mounted, rescan it.
            this.rescan();
            break;
          }
        }
      }
    }
  }

  var rootPath = this.getCurrentRootPath();
  var rootType = PathUtil.getRootType(rootPath);

  // If the path is on drive, reduce to the Drive's mount point.
  if (rootType === RootType.DRIVE)
    rootPath = RootDirectory.DRIVE;

  // When the volume where we are is unmounted, fallback to
  // DEFAULT_DIRECTORY.
  // Note: during the initialization, rootType can be undefined.
  if (rootType && !this.volumeManager_.getVolumeInfo(rootPath))
    this.changeDirectory(PathUtil.DEFAULT_DIRECTORY);
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
 * @param {function(Event)} onSearchRescan Function that will be called when the
 *     search directory is rescanned (i.e. search results are displayed).
 * @param {function()} onClearSearch Function to be called when search state
 *     gets cleared.
 * TODO(olege): Change callbacks to events.
 */
DirectoryModel.prototype.search = function(query,
                                           onSearchRescan,
                                           onClearSearch) {
  query = query.trimLeft();

  this.clearSearch_();

  var currentDirEntry = this.getCurrentDirEntry();
  if (!currentDirEntry) {
    // Not yet initialized. Do nothing.
    return;
  }

  if (!query) {
    if (this.isSearching()) {
      var newDirContents = new DirectoryContentsBasic(
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
  // A search initiated from directories in Drive or special search results
  // should trigger Drive search.
  var newDirContents;
  if (!this.isDriveOffline() &&
      PathUtil.isDriveBasedPath(currentDirEntry.fullPath)) {
    // Drive search is performed over the whole drive, so pass  drive root as
    // |directoryEntry|.
    newDirContents = new DirectoryContentsDriveSearch(
        this.currentFileListContext_,
        currentDirEntry,
        this.currentDirContents_.getLastNonSearchDirectoryEntry(),
        query);
  } else {
    newDirContents = new DirectoryContentsLocalSearch(
        this.currentFileListContext_, currentDirEntry, query);
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
  var query = opt_query || '';

  this.clearSearch_();

  this.onSearchCompleted_ = null;
  this.onClearSearch_ = null;

  var onDriveDirectoryResolved = function(driveRoot) {
    if (!driveRoot || driveRoot == DirectoryModel.fakeDriveEntry_) {
      // Drive root not available or not ready. onVolumeInfoListUpdated_()
      // handles the rescan if necessary.
      driveRoot = null;
    }

    var specialSearchType = PathUtil.getRootType(path);
    var searchOption;
    var dirEntry;
    if (specialSearchType == RootType.DRIVE_OFFLINE) {
      dirEntry = DirectoryModel.fakeDriveOfflineEntry_;
      searchOption =
          DriveMetadataSearchContentScanner.SearchType.SEARCH_OFFLINE;
    } else if (specialSearchType == RootType.DRIVE_SHARED_WITH_ME) {
      dirEntry = DirectoryModel.fakeDriveSharedWithMeEntry_;
      searchOption =
          DriveMetadataSearchContentScanner.SearchType.SEARCH_SHARED_WITH_ME;
    } else if (specialSearchType == RootType.DRIVE_RECENT) {
      dirEntry = DirectoryModel.fakeDriveRecentEntry_;
      searchOption =
          DriveMetadataSearchContentScanner.SearchType.SEARCH_RECENT_FILES;

    } else {
      // Unknown path.
      this.changeDirectory(PathUtil.DEFAULT_DIRECTORY);
      return;
    }

    var newDirContents = new DirectoryContentsDriveSearchMetadata(
        this.currentFileListContext_,
        driveRoot,
        dirEntry,
        query,
        searchOption);
    var previous = this.currentDirContents_.getDirectoryEntry();
    this.clearAndScan_(newDirContents);

    var e = new cr.Event('directory-changed');
    e.previousDirEntry = previous;
    e.newDirEntry = dirEntry;
    this.dispatchEvent(e);
  }.bind(this);

  this.resolveDirectory(DirectoryModel.fakeDriveEntry_.fullPath,
                        onDriveDirectoryResolved /* success */,
                        function() {} /* failed */);
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
