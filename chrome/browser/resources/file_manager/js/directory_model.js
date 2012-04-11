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
 * @param {boolean} singleSelection True if only one file could be selected
 *                                  at the time.
 * @param {boolean} showGData Defines whether GData root should be should
 *   (regardless of its mounts status).
 */
function DirectoryModel(root, singleSelection, showGData) {
  this.root_ = root;
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
  this.autoSelectIndex_ = 0;

  this.rootsList_ = new cr.ui.ArrayDataModel([]);
  this.rootsListSelection_ = new cr.ui.ListSingleSelectionModel();
  this.rootsListSelection_.addEventListener(
      'change', this.onRootsSelectionChanged_.bind(this));

  // True if we should filter out files that start with a dot.
  this.filterHidden_ = true;

  // Readonly status for removable volumes.
  this.readonly_ = false;
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
DirectoryModel.GDATA_DIRECTORY = 'gdata';

DirectoryModel.prototype = {
  __proto__: cr.EventTarget.prototype,

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

  get rootType() {
    return DirectoryModel.getRootType(this.currentEntry.fullPath);
  },

  get rootName() {
    return DirectoryModel.getRootName(this.currentEntry.fullPath);
  },

  get rootEntry() {
    return this.rootsList.item(this.rootsListSelection.selectedIndex);
  },

  /**
   * True if current directory is read only. Value may be set
   * for directories on a removable device.
   * @type {boolean}
   */
  get readonly() {
    switch (this.rootType) {
      case DirectoryModel.RootType.REMOVABLE:
        return this.readonly_;
      case DirectoryModel.RootType.ARCHIVE:
        return true;
      case DirectoryModel.RootType.DOWNLOADS:
        return false;
      case DirectoryModel.RootType.GDATA:
        return false;
      default:
        return true;
    }
  },

  set readonly(value) {
    if (this.rootType == DirectoryModel.RootType.REMOVABLE) {
      this.readonly_ = !!value;
    }
  },

  get isSystemDirectoy() {
    var path = this.currentEntry.fullPath;
    return path == '/' ||
           path == '/' + DirectoryModel.REMOVABLE_DIRECTORY ||
           path == '/' + DirectoryModel.ARCHIVE_DIRECTORY;
  },

  get filterHidden() {
    return this.filterHidden_;
  },

  set filterHidden(value) {
    if (this.filterHidden_ != value) {
      this.filterHidden_ = value;
      this.rescan();
    }
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
   * Names of selected files.
   * @type {Array.<string>}
   */
  get selectedNames() {
    var indexes = this.fileListSelection_.selectedIndexes;
    var dataModel = this.fileList_;
    if (dataModel) {
      return indexes.map(function(i) {
        return dataModel.item(i).name;
      });
    }
    return [];
  },

  set selectedNames(value) {
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
  },

  /**
   * Lead item file name.
   * @type {string?}
   */
  get leadName() {
    var index = this.fileListSelection_.leadIndex;
    return index >= 0 && this.fileList_.item(index).name;
  },

  set leadName(value) {
    for (var i = 0; i < this.fileList_.length; i++) {
      if (this.fileList_.item(i).name == value) {
        this.fileListSelection_.leadIndex = i;
        return;
      }
    }
  },

  /**
   * Schedule rescan with delay. If another rescan has been scheduled does
   * nothing. Designed to handle directory change notification. File operation
   * may cause a few notifications what should cause a single refresh.
   */
  rescanLater: function() {
    if (this.rescanTimeout_)
      return;  // Rescan already scheduled.

    var self = this;
    function onTimeout() {
      self.rescanTimeout_ = undefined;
      self.rescan();
    }
    this.rescanTimeout_ = setTimeout(onTimeout, SIMULTANEOUS_RESCAN_INTERVAL);
  },

  /**
   * Rescan current directory. May be called indirectly through rescanLater or
   * directly in order to reflect user action.
   */
  rescan: function() {
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
  },

  createScanner_: function(list, successCallback) {
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
        this.filterHidden_);
  },

  replaceFileList_: function(entries) {
    cr.dispatchSimpleEvent(this, 'begin-update-files');
    this.fileListSelection_.beginChange();

    var selectedNames = this.selectedNames;
    // Restore leadIndex in case leadName no longer exists.
    var leadIndex = this.fileListSelection_.leadIndex;
    var leadName = this.leadName;

    var spliceArgs = [].slice.call(entries);
    spliceArgs.unshift(0, this.fileList_.length);
    this.fileList_.splice.apply(this.fileList_, spliceArgs);

    this.selectedNames = selectedNames;
    this.fileListSelection_.leadIndex = leadIndex;
    this.leadName = leadName;
    this.fileListSelection_.endChange();
    cr.dispatchSimpleEvent(this, 'end-update-files');
  },

  /**
   * Cancels waiting and scheduled rescans and starts new scan.
   *
   * If the scan completes successfully on the first attempt, the callback will
   * be invoked and a 'scan-completed' event will be dispatched.  If the scan
   * fails for any reason, we'll periodically retry until it succeeds (and then
   * send a 'rescan-complete' event) or is cancelled or replaced by another
   * scan.
   *
   * @param {Function} callback Called if scan completes on the first attempt.
   *   Note that this will NOT be called if the scan fails but later succeeds.
   */
  scan_: function(callback) {
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

  /**
   * Delete the list of files and directories from filesystem and
   * update the file list.
   * @param {Array.<Entry>} entries Entries to delete.
   * @param {Function} opt_callback Called when finished.
   */
  deleteEntries: function(entries, opt_callback) {
    var downcount = entries.length + 1;

    var onComplete = opt_callback ? function() {
      if (--downcount == 0)
        opt_callback();
    } : function() {};

    const fileList = this.fileList_;
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
  },

  onEntryChanged: function(name) {
    var currentEntry = this.currentEntry;
    var dm = this.fileList_;
    var self = this;

    function onEntryFound(entry) {
      self.prefetchCacheForSorting_([entry], function() {
        // Do nothing if current directory changed during async operations.
        if (self.currentEntry != currentEntry)
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
      if (self.currentEntry != currentEntry)
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
  },

  findIndexByName_: function(name) {
    var dm = this.fileList_;
    for (var i = 0; i < dm.length; i++)
      if (dm.item(i).name == name)
        return i;
    return -1;
  },

  /**
   * Rename the entry in the filesystem and update the file list.
   * @param {Entry} entry Entry to rename.
   * @param {string} newName
   * @param {Function} errorCallback Called on error.
   * @param {Function} opt_successCallback Called on success.
   */
  renameEntry: function(entry, newName, errorCallback, opt_successCallback) {
    var self = this;
    function onSuccess(newEntry) {
      self.prefetchCacheForSorting_([newEntry], function() {
        const fileList = self.fileList_;
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
    entry.moveTo(this.currentEntry, newName,
                 onSuccess, errorCallback);
  },

  /**
   * Checks if current directory contains a file or directory with this name.
   * @param {string} newName Name to check.
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
    const self = this;
    function onSuccess(newEntry) {
      self.prefetchCacheForSorting_([newEntry], function() {
        const fileList = self.fileList_;
        var existing = fileList.slice().filter(
            function(e) { return e.name == name; });

        if (existing.length) {
          self.selectEntry(name);
          successCallback(existing[0]);
        } else {
          self.fileListSelection.beginChange();
          fileList.splice(0, 0, newEntry);
          self.selectEntry(name);
          self.fileListSelection.endChange();
          successCallback(newEntry);
        }
      });
    }

    this.currentEntry.getDirectory(name, {create: true, exclusive: true},
                                   onSuccess, errorCallback);
  },

  /**
   * Changes directory. Causes 'directory-change' event.
   *
   * @param {string} path New current directory path.
   */
  changeDirectory: function(path) {
    var onDirectoryResolved = function(dirEntry) {
      var autoSelect = this.selectIndex.bind(this, this.autoSelectIndex_);
      this.changeDirectoryEntry_(dirEntry, autoSelect, false);
    }.bind(this);

    if (this.unmountedGDataEntry_ &&
        DirectoryModel.getRootType(path) == DirectoryModel.RootType.GDATA) {
      this.readonly_ = true;
      // TODO(kaznacheeev): Currently if path points to some GData subdirectory
      // and GData is not mounted we will change to the fake GData root and
      // ignore the rest of the path. Consider remembering the path and
      // changing to it once GDdata is mounted. This is only relevant for cases
      // when we open the File Manager with an URL pointing to GData (e.g. via
      // a bookmark).
      onDirectoryResolved(this.unmountedGDataEntry_);
      return;
    }

    this.readonly_ = false;
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
   * @param {function} action Action executed if the directory loads
   *    successfully.  By default selects the first item (unless it's a save
   *    dialog).
   * @param {boolean} initial True if it comes from setupPath and
   *                          false if caused by an user action.
   */
  changeDirectoryEntry_: function(dirEntry, action, initial) {
    var previous = this.currentEntry;
    this.currentDirEntry_ = dirEntry;
    function onRescanComplete() {
      action();
      // For tests that open the dialog to empty directories, everything
      // is loaded at this point.
      chrome.test.sendMessage('directory-change-complete');
    }
    this.updateRootsListSelection_();
    this.scan_(onRescanComplete);

    var e = new cr.Event('directory-changed');
    e.previousDirEntry = previous;
    e.newDirEntry = dirEntry;
    e.initial = initial;
    this.dispatchEvent(e);
  },

  /**
   * Change the state of the model to reflect the specified path (either a
   * file or directory).
   *
   * @param {string} path The root path to use
   * @param {Function=} opt_loadedCallback Invoked when the entire directory
   *     has been loaded and any default file selected.  If there are any
   *     errors loading the directory this will not get called (even if the
   *     directory loads OK on retry later). Will NOT be called if another
   *     directory change happened while setupPath was in progress.
   * @param {Function=} opt_pathResolveCallback Invoked as soon as the path has
   *     been resolved, and called with the base and leaf portions of the path
   *     name, and a flag indicating if the entry exists. Will be called even
   *     if another directory change happened while setupPath was in progress,
   *     but will pass |false| as |exist| parameter.
   */
  setupPath: function(path, opt_loadedCallback, opt_pathResolveCallback) {
    var overridden = false;
    function onExternalDirChange() { overridden = true }
    this.addEventListener('directory-changed', onExternalDirChange);

    var resolveCallback = function(exists) {
      this.removeEventListener('directory-changed', onExternalDirChange);
      if (opt_pathResolveCallback)
        opt_pathResolveCallback(baseName, leafName, exists && !overridden);
    }.bind(this);

    var changeDirectoryEntry = function(entry, callback, initial, exists) {
      resolveCallback(exists);
      if (!overridden)
        this.changeDirectoryEntry_(entry, callback, initial);
    }.bind(this);

    var INITIAL = true;
    var EXISTS = true;

    // Split the dirname from the basename.
    var ary = path.match(/^(?:(.*)\/)?([^\/]*)$/);
    var autoSelect = function() {
      this.selectIndex(this.autoSelectIndex_);
      if (opt_loadedCallback)
        opt_loadedCallback();
    }.bind(this);

    if (!ary) {
      console.warn('Unable to split default path: ' + path);
      changeDirectoryEntry(this.root_, autoSelect, INITIAL, !EXISTS);
      return;
    }

    var baseName = ary[1];
    var leafName = ary[2];

    function onLeafFound(baseDirEntry, leafEntry) {
      if (leafEntry.isDirectory) {
        baseName = path;
        leafName = '';
        changeDirectoryEntry(leafEntry, autoSelect, INITIAL, EXISTS);
        return;
      }

      // Leaf is an existing file, cd to its parent directory and select it.
      changeDirectoryEntry(baseDirEntry,
                           function() {
                             this.selectEntry(leafEntry.name);
                             if (opt_loadedCallback)
                               opt_loadedCallback();
                           }.bind(this),
                           !INITIAL /*HACK*/,
                           EXISTS);
      // TODO(kaznacheev): Fix history.replaceState for the File Browser and
      // change !INITIAL to INITIAL. Passing |false| makes things
      // less ugly for now.
    }

    function onLeafError(baseDirEntry, err) {
      // Usually, leaf does not exist, because it's just a suggested file name.
      if (err.code != FileError.NOT_FOUND_ERR)
        console.log('Unexpected error resolving default leaf: ' + err);
      changeDirectoryEntry(baseDirEntry, autoSelect, INITIAL, !EXISTS);
    }

    var onBaseError = function(err) {
      console.log('Unexpected error resolving default base "' +
                  baseName + '": ' + err);
      if (path != '/' + DirectoryModel.DOWNLOADS_DIRECTORY) {
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
        changeDirectoryEntry(baseDirEntry, autoSelect, INITIAL, !EXISTS);
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

  setupDefaultPath: function(opt_callback) {
    var overridden = false;
    function onExternalDirChange() { overridden = true }
    this.addEventListener('directory-changed', onExternalDirChange);

    this.getDefaultDirectory_(function(path) {
      this.removeEventListener('directory-changed', onExternalDirChange);
      if (!overridden)
        this.setupPath(path, opt_callback);
    }.bind(this));
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
      cacheFunction = this.cacheEntryDateAndSize;
    } else if (field == 'cachedSize_') {
      cacheFunction = this.cacheEntryDateAndSize;
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
        cacheFunction(entry, onCacheDone, onCacheDone);
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
   * Get root entries asynchronously.
   * @param {function(Array.<Entry>} callback Called when roots are resolved.
   * @param {boolean} resolveGData See comment for updateRoots.
   */
  resolveRoots_: function(callback, resolveGData) {
    var groups = {
      downloads: null,
      archives: null,
      removables: null,
      gdata: null
    };

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

    function onDownloads(entry) {
      groups.downloads = [entry];
      done();
    }

    function onDownloadsError(error) {
      groups.downloads = [];
      done();
    }

    var self = this;

    function onGData(entry) {
      console.log('GData found:', entry);
      self.unmountedGDataEntry_ = null;
      groups.gdata = [entry];
      done();
    }

    function onGDataError(error) {
      console.log('GData error: ', error);
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
  },

  /**
  * @param {function} opt_callback Called when all roots are resolved.
  * @param {boolean} opt_resolveGData If true GData should be resolved for real,
  *                                   If false a stub entry should be created.
  */
  updateRoots: function(opt_callback, opt_resolveGData) {
    console.log('resolving roots');
    var self = this;
    this.resolveRoots_(function(rootEntries) {
      console.log('resolved roots:', rootEntries);
      var dm = self.rootsList_;
      var args = [0, dm.length].concat(rootEntries);
      dm.splice.apply(dm, args);

      self.updateRootsListSelection_();

      if (opt_callback)
        opt_callback();
    }, opt_resolveGData);
  },

  onRootsSelectionChanged_: function(event) {
    var root = this.rootsList.item(this.rootsListSelection.selectedIndex);
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

DirectoryModel.getRootName = function(path) {
  var root = DirectoryModel.getRootPath(path);
  var index = root.lastIndexOf('/');
  return index == -1 ? root : root.substring(index + 1);
};

DirectoryModel.getRootType = function(path) {
  function isTop(dir) {
    return path.substr(1, dir.length) == dir;
  }

  if (isTop(DirectoryModel.DOWNLOADS_DIRECTORY))
    return DirectoryModel.RootType.DOWNLOADS;
  else if (isTop(DirectoryModel.GDATA_DIRECTORY))
    return DirectoryModel.RootType.GDATA;
  else if (isTop(DirectoryModel.ARCHIVE_DIRECTORY))
    return DirectoryModel.RootType.ARCHIVE;
  else if(isTop(DirectoryModel.REMOVABLE_DIRECTORY))
    return DirectoryModel.RootType.REMOVABLE;
  return '';
};

DirectoryModel.isRootPath = function(path) {
  if (path[path.length - 1] == '/')
    path = path.substring(0, path.length - 1);
  return DirectoryModel.getRootPath(path) == path;
};

/**
 * @constructor
 * @param {DirectoryEntry} dir Directory to scan.
 * @param {Array.<Entry>|cr.ui.ArrayDataModel} list Target to put the files.
 * @param {Function} successCallback Callback to call when (and if) scan
 *     successfully completed.
 * @param {Function} errorCallback Callback to call in case of IO error.
 * @param {function(Array.<Entry>):void, Function)} preprocessChunk
 *     Callback to preprocess each chunk of files.
 * @param {boolean} filterHidden True if files started with dots are ignored.
 */
DirectoryModel.Scanner = function(dir, list, successCallback, errorCallback,
                                  preprocessChunk, filterHidden) {
  this.cancelled_ = false;
  this.list_ = list;
  this.dir_ = dir;
  this.reader_ = null;
  this.filterHidden_ = !!filterHidden;
  this.preprocessChunk_ = preprocessChunk;
  this.successCallback_ = successCallback;
  this.errorCallback_ = errorCallback;
};

DirectoryModel.Scanner.prototype = {
  __proto__: cr.EventTarget.prototype,

  cancel: function() {
    this.cancelled_ = true;
  },

  run: function() {
    metrics.startInterval('DirectoryScan');

    this.reader_ = this.dir_.createReader();
    this.readNextChunk_();
  },

  readNextChunk_: function() {
    this.reader_.readEntries(this.onChunkComplete_.bind(this),
                             this.errorCallback_);
  },

  onChunkComplete_: function(entries) {
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

    // Hide files that start with a dot ('.').
    // TODO(rginda): User should be able to override this. Support for other
    // commonly hidden patterns might be nice too.
    if (this.filterHidden_) {
      spliceArgs = spliceArgs.filter(function(e) {
        return e.name.substr(0, 1) != '.';
      });
    }

    var self = this;
    self.preprocessChunk_(spliceArgs, function() {
      spliceArgs.unshift(0, 0);  // index, deleteCount
      self.list_.splice.apply(self.list_, spliceArgs);

      // Keep reading until entries.length is 0.
      self.readNextChunk_();
    });
  },

  recordMetrics_: function() {
    metrics.recordInterval('DirectoryScan');
    if (this.dir_.fullPath ==
        '/' + DirectoryModel.DOWNLOADS_DIRECTORY) {
      metrics.recordMediumCount("DownloadsCount", this.list_.length);
    }
  }
};

