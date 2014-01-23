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
 * @constructor
 */
function DirectoryModel(singleSelection, fileFilter, fileWatcher,
                        metadataCache, volumeManager) {
  this.fileListSelection_ = singleSelection ?
      new cr.ui.ListSingleSelectionModel() : new cr.ui.ListSelectionModel();

  this.runningScan_ = null;
  this.pendingScan_ = null;
  this.rescanTime_ = null;
  this.scanFailures_ = 0;
  this.changeDirectorySequence_ = 0;

  this.fileFilter_ = fileFilter;
  this.fileFilter_.addEventListener('changed',
                                    this.onFilterChanged_.bind(this));

  this.currentFileListContext_ = new FileListContext(
      fileFilter, metadataCache);
  this.currentDirContents_ =
      DirectoryContents.createForDirectory(this.currentFileListContext_, null);

  this.volumeManager_ = volumeManager;
  this.volumeManager_.volumeInfoList.addEventListener(
      'splice', this.onVolumeInfoListUpdated_.bind(this));

  this.fileWatcher_ = fileWatcher;
  this.fileWatcher_.addEventListener(
      'watcher-directory-changed',
      this.onWatcherDirectoryChanged_.bind(this));
}

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
 * @return {?RootType} Root type of current root, or null if not found.
 */
DirectoryModel.prototype.getCurrentRootType = function() {
  var entry = this.currentDirContents_.getDirectoryEntry();
  if (!entry)
    return null;

  var locationInfo = this.volumeManager_.getLocationInfo(entry);
  if (!locationInfo)
    return null;

  return locationInfo.rootType;
};

/**
 * @return {boolean} True if the current directory is read only. If there is
 *     no entry set, then returns true.
 */
DirectoryModel.prototype.isReadOnly = function() {
  return this.getCurrentDirEntry() ? this.volumeManager_.getLocationInfo(
      this.getCurrentDirEntry()).isReadOnly : true;
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

  // If dispatchNeeded is true, we should ensure the change event is
  // dispatched.
  var dispatchNeeded = updateFunc();

  // Check if the change event is dispatched in the endChange function
  // or not.
  var eventDispatched = function() { dispatchNeeded = false; };
  selection.addEventListener('change', eventDispatched);
  selection.endChange();
  selection.removeEventListener('change', eventDispatched);

  // If the change event have been already dispatched, dispatchNeeded is false.
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
 * @return {Array.<Entry>} Array of selected entries..
 * @private
 */
DirectoryModel.prototype.getSelectedEntries_ = function() {
  var indexes = this.fileListSelection_.selectedIndexes;
  var fileList = this.getFileList();
  if (fileList) {
    return indexes.map(function(i) {
      return fileList.item(i);
    });
  }
  return [];
};

/**
 * @param {Array.<Entry>} value List of selected entries.
 * @private
 */
DirectoryModel.prototype.setSelectedEntries_ = function(value) {
  var indexes = [];
  var fileList = this.getFileList();
  var urls = util.entriesToURLs(value);

  for (var i = 0; i < fileList.length; i++) {
    if (urls.indexOf(fileList.item(i).toURL()) !== -1)
      indexes.push(i);
  }
  this.fileListSelection_.selectedIndexes = indexes;
};

/**
 * @return {Entry} Lead entry.
 * @private
 */
DirectoryModel.prototype.getLeadEntry_ = function() {
  var index = this.fileListSelection_.leadIndex;
  return index >= 0 && this.getFileList().item(index);
};

/**
 * @param {Entry} value The new lead entry.
 * @private
 */
DirectoryModel.prototype.setLeadEntry_ = function(value) {
  var fileList = this.getFileList();
  for (var i = 0; i < fileList.length; i++) {
    if (util.isSameEntry(fileList.item(i), value)) {
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
    if (this.pendingRescan_) {
      this.rescanSoon();
      this.pendingRescan_ = false;
      return true;
    }
    return false;
  }.bind(this);

  var onSuccess = function() {
    // Record metric for Downloads directory.
    if (!dirContents.isSearch()) {
      var locationInfo =
          this.volumeManager_.getLocationInfo(dirContents.getDirectoryEntry());
      if (locationInfo.volumeInfo.volumeType === util.VolumeType.DOWNLOADS &&
          locationInfo.isRootEntry) {
        metrics.recordMediumCount('DownloadsCount',
                                  dirContents.fileList_.length);
      }
    }

    this.runningScan_ = null;
    successCallback();
    this.scanFailures_ = 0;
    maybeRunPendingRescan();
  }.bind(this);

  var onFailure = function() {
    this.runningScan_ = null;
    this.scanFailures_++;
    failureCallback();

    if (maybeRunPendingRescan())
      return;

    if (this.scanFailures_ <= 1)
      this.rescanLater();
  }.bind(this);

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
    var selectedEntries = this.getSelectedEntries_();
    var selectedIndices = this.fileListSelection_.selectedIndexes;

    // Restore leadIndex in case leadName no longer exists.
    var leadIndex = this.fileListSelection_.leadIndex;
    var leadEntry = this.getLeadEntry_();

    this.currentDirContents_ = dirContents;
    dirContents.replaceContextFileList();

    this.setSelectedEntries_(selectedEntries);
    this.fileListSelection_.leadIndex = leadIndex;
    this.setLeadEntry_(leadEntry);

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
      if (!util.isSameEntry(this.getCurrentDirEntry(), parentEntry)) {
        // Do nothing if current directory changed during async operations.
        return;
      }
      this.currentDirContents_.prefetchMetadata([entry], function() {
        if (!util.isSameEntry(this.getCurrentDirEntry(), parentEntry)) {
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
      this.changeDirectoryEntry(newEntry);

    // Look for the old entry.
    // If the entry doesn't exist in the list, it has been updated from
    // outside (probably by directory rescan).
    var index = this.findIndexByEntry_(oldEntry);
    if (index >= 0) {
      // Update the content list and selection status.
      var wasSelected = this.fileListSelection_.getIndexSelected(index);
      this.updateSelectionAndPublishEvent_(this.fileListSelection_, function() {
        this.fileListSelection_.setIndexSelected(index, false);
        this.getFileList().splice(index, 1, newEntry);
        if (wasSelected) {
          // We re-search the index, because splice may trigger sorting so that
          // index may be stale.
          this.fileListSelection_.setIndexSelected(
              this.findIndexByEntry_(newEntry), true);
        }
        return true;
      }.bind(this));
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

  var tracker = this.createDirectoryChangeTracker();
  tracker.start();

  var onSuccess = function(newEntry) {
    // Do not change anything or call the callback if current
    // directory changed.
    tracker.stop();
    if (tracker.hasChanged)
      return;

    var existing = this.getFileList().slice().filter(
        function(e) {return e.name == name;});

    if (existing.length) {
      this.selectEntry(newEntry);
      successCallback(existing[0]);
    } else {
      this.fileListSelection_.beginChange();
      this.getFileList().splice(0, 0, newEntry);
      this.selectEntry(newEntry);
      this.fileListSelection_.endChange();
      successCallback(newEntry);
    }
  };

  this.currentDirContents_.createDirectory(name, onSuccess.bind(this),
                                           errorCallback);
};

/**
 * @param {DirectoryEntry} dirEntry The entry of the new directory.
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
  this.clearAndScan_(
      DirectoryContents.createForDirectory(this.currentFileListContext_,
                                           dirEntry),
      onScanComplete.bind(this));
};

/**
 * Change the current directory to the directory represented by
 * a DirectoryEntry or a fake entry.
 *
 * Dispatches the 'directory-changed' event when the directory is successfully
 * changed.
 *
 * @param {DirectoryEntry|Object} dirEntry The entry of the new directory to
 *     be opened.
 * @param {function()=} opt_callback Executed if the directory loads
 *     successfully.
 */
DirectoryModel.prototype.changeDirectoryEntry = function(
    dirEntry, opt_callback) {
  if (util.isFakeEntry(dirEntry)) {
    this.specialSearch(dirEntry);
    if (opt_callback)
      opt_callback();
    return;
  }

  // Increment the sequence value.
  this.changeDirectorySequence_++;

  this.fileWatcher_.changeWatchedDirectory(dirEntry, function(sequence) {
    if (this.changeDirectorySequence_ !== sequence)
      return;
    var previous = this.currentDirContents_.getDirectoryEntry();
    this.clearSearch_();
    this.changeDirectoryEntrySilent_(dirEntry, opt_callback);

    var e = new Event('directory-changed');
    e.previousDirEntry = previous;
    e.newDirEntry = dirEntry;
    this.dispatchEvent(e);
  }.bind(this, this.changeDirectorySequence_));
};

/**
 * Creates an object which could say whether directory has changed while it has
 * been active or not. Designed for long operations that should be cancelled
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
 * @param {Entry} entry Entry to be selected.
 */
DirectoryModel.prototype.selectEntry = function(entry) {
  var fileList = this.getFileList();
  for (var i = 0; i < fileList.length; i++) {
    if (fileList.item(i).toURL() === entry.toURL()) {
      this.selectIndex(i);
      return;
    }
  }
};

/**
 * @param {Array.<string>} entries Array of entries.
 */
DirectoryModel.prototype.selectEntries = function(entries) {
  // URLs are needed here, since we are comparing Entries by URLs.
  var urls = util.entriesToURLs(entries);
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
 * @param {Event} event Event of VolumeInfoList's 'splice'.
 * @private
 */
DirectoryModel.prototype.onVolumeInfoListUpdated_ = function(event) {
  // When the volume where we are is unmounted, fallback to the default volume's
  // root. If current directory path is empty, stop the fallback
  // since the current directory is initializing now.
  if (this.getCurrentDirEntry() &&
      !this.volumeManager_.getVolumeInfo(this.getCurrentDirEntry())) {
    this.volumeManager_.getDefaultDisplayRoot(function(displayRoot) {
      this.changeDirectoryEntry(displayRoot);
    }.bind(this));
  }
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
 * name search over current directory will be performed.
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
      var newDirContents = DirectoryContents.createForDirectory(
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
  var isDriveOffline = this.volumeManager_.getDriveConnectionState().type ===
      util.DriveConnectionType.OFFLINE;
  var locationInfo = this.volumeManager_.getLocationInfo(currentDirEntry);
  if (!isDriveOffline && locationInfo && locationInfo.isDriveBased) {
    // Drive search is performed over the whole drive, so pass  drive root as
    // |directoryEntry|.
    newDirContents = DirectoryContents.createForDriveSearch(
        this.currentFileListContext_,
        currentDirEntry,
        this.currentDirContents_.getLastNonSearchDirectoryEntry(),
        query);
  } else {
    newDirContents = DirectoryContents.createForLocalSearch(
        this.currentFileListContext_, currentDirEntry, query);
  }
  this.clearAndScan_(newDirContents);
};

/**
 * Performs special search and displays results. e.g. Drive files available
 * offline, shared-with-me files, recently modified files.
 * @param {Object} fakeEntry Fake entry representing a special search.
 * @param {string=} opt_query Query string used for the search.
 */
DirectoryModel.prototype.specialSearch = function(fakeEntry, opt_query) {
  var query = opt_query || '';

  // Increment the sequence value.
  this.changeDirectorySequence_++;

  this.clearSearch_();
  this.onSearchCompleted_ = null;
  this.onClearSearch_ = null;

  // Obtains a volume information.
  // TODO(hirono): Obtain the proper profile's volume information.
  var volumeInfo = this.volumeManager_.getCurrentProfileVolumeInfo(
      util.VolumeType.DRIVE);
  if (!volumeInfo) {
    // It seems that the volume is already unmounted or drive is disable.
    return;
  }

  var onDriveDirectoryResolved = function(sequence, driveRoot) {
    if (this.changeDirectorySequence_ !== sequence)
      return;

    var locationInfo = this.volumeManager_.getLocationInfo(fakeEntry);
    if (!locationInfo)
      return;

    var searchOption;
    switch (locationInfo.rootType) {
      case RootType.DRIVE_OFFLINE:
        searchOption =
            DriveMetadataSearchContentScanner.SearchType.SEARCH_OFFLINE;
        break;
      case RootType.DRIVE_SHARED_WITH_ME:
        searchOption =
            DriveMetadataSearchContentScanner.SearchType.SEARCH_SHARED_WITH_ME;
        break;
      case RootType.DRIVE_RECENT:
        searchOption =
            DriveMetadataSearchContentScanner.SearchType.SEARCH_RECENT_FILES;
        break;
      default:
        // Unknown special search entry.
        throw new Error('Unknown special search type.');
    }

    var newDirContents = DirectoryContents.createForDriveMetadataSearch(
        this.currentFileListContext_,
        fakeEntry, driveRoot, query, searchOption);
    var previous = this.currentDirContents_.getDirectoryEntry();
    this.clearAndScan_(newDirContents);

    var e = new Event('directory-changed');
    e.previousDirEntry = previous;
    e.newDirEntry = fakeEntry;
    this.dispatchEvent(e);
  }.bind(this, this.changeDirectorySequence_);

  volumeInfo.resolveDisplayRoot(
      onDriveDirectoryResolved /* success */, function() {} /* failed */);
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
