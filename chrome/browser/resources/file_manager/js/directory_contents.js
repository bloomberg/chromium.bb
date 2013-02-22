// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {MetadataCache} metadataCache Metadata cache service.
 * @param {cr.ui.ArrayDataModel} fileList The file list.
 * @param {boolean} showHidden If files starting with '.' are shown.
 * @constructor
 */
function FileListContext(metadataCache, fileList, showHidden) {
  /**
   * @type {MetadataCache}
   */
  this.metadataCache = metadataCache;
  /**
   * @type {cr.ui.ArrayDataModel}
   */
  this.fileList = fileList;
  /**
   * @type Object.<string, Function>
   * @private
   */
  this.filters_ = {};
  this.setFilterHidden(!showHidden);

  // Do not show entries marked as 'deleted'.
  this.addFilter('deleted', function(entry) {
    var internal = this.metadataCache.getCached(entry, 'internal');
    return !(internal && internal.deleted);
  }.bind(this));
}

/**
 * @param {string} name Filter identifier.
 * @param {function(Entry)} callback A filter — a function receiving an Entry,
 *     and returning bool.
 */
FileListContext.prototype.addFilter = function(name, callback) {
  this.filters_[name] = callback;
};

/**
 * @param {string} name Filter identifier.
 */
FileListContext.prototype.removeFilter = function(name) {
  delete this.filters_[name];
};

/**
 * @param {boolean} value If do not show hidden files.
 */
FileListContext.prototype.setFilterHidden = function(value) {
  if (value) {
    this.addFilter(
        'hidden',
        function(entry) {return entry.name.substr(0, 1) !== '.';}
    );
  } else {
    this.removeFilter('hidden');
  }
};

/**
 * @return {boolean} If the files with names starting with "." are not shown.
 */
FileListContext.prototype.isFilterHiddenOn = function() {
  return 'hidden' in this.filters_;
};

/**
 * @param {Entry} entry File entry.
 * @return {boolean} True if the file should be shown, false otherwise.
 */
FileListContext.prototype.filter = function(entry) {
  for (var name in this.filters_) {
    if (!this.filters_[name](entry))
      return false;
  }
  return true;
};


/**
 * This class is responsible for scanning directory (or search results),
 * and filling the fileList. Different descendants handle various types of
 * directory contents shown: basic directory, drive search results, local search
 * results.
 * @param {FileListContext} context The file list context.
 * @constructor
 * @extends {cr.EventTarget}
 */
function DirectoryContents(context) {
  this.context_ = context;
  this.fileList_ = context.fileList;
  this.scanCompletedCallback_ = null;
  this.scanFailedCallback_ = null;
  this.scanCancelled_ = false;
  this.filter_ = context.filter.bind(context);
  this.allChunksFetched_ = false;
  this.pendingMetadataRequests_ = 0;
  this.fileList_.prepareSort = this.prepareSort_.bind(this);
}

/**
 * DirectoryContents extends cr.EventTarget.
 */
DirectoryContents.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Create the copy of the object, but without scan started.
 * @return {DirectoryContents} Object copy.
 */
DirectoryContents.prototype.clone = function() {
  return new DirectoryContents(this.context_);
};

/**
 * Use a given fileList instead of the fileList from the context.
 * @param {Array|cr.ui.ArrayDataModel} fileList The new file list.
 */
DirectoryContents.prototype.setFileList = function(fileList) {
  this.fileList_ = fileList;
  this.fileList_.prepareSort = this.prepareSort_.bind(this);
};

/**
 * Use the filelist from the context and replace its contents with the entries
 * from the current fileList.
 */
DirectoryContents.prototype.replaceContextFileList = function() {
  if (this.context_.fileList !== this.fileList_) {
    var spliceArgs = [].slice.call(this.fileList_);
    var fileList = this.context_.fileList;
    spliceArgs.unshift(0, fileList.length);
    fileList.splice.apply(fileList, spliceArgs);
    this.fileList_ = fileList;
  }
};

/**
 * @return {string} The path.
 */
DirectoryContents.prototype.getPath = function() {
  throw 'Not implemented.';
};

/**
 * @return {boolean} If the scan is active.
 */
DirectoryContents.prototype.isScanning = function() {
  return !this.scanCancelled_ &&
         (!this.allChunksFetched_ || this.pendingMetadataRequests_ > 0);
};

/**
 * @return {boolean} True if search results (drive or local).
 */
DirectoryContents.prototype.isSearch = function() {
  return false;
};

/**
 * @return {DirectoryEntry} A DirectoryEntry for current directory. In case of
 *     search -- the top directory from which search is run.
 */
DirectoryContents.prototype.getDirectoryEntry = function() {
  throw 'Not implemented.';
};

/**
 * @return {DirectoryEntry} A DirectoryEntry for the last non search contents.
 */
DirectoryContents.prototype.getLastNonSearchDirectoryEntry = function() {
  throw 'Not implemented.';
};

/**
 * Start directory scan/search operation. Either 'scan-completed' or
 * 'scan-failed' event will be fired upon completion.
 */
DirectoryContents.prototype.scan = function() {
  throw 'Not implemented.';
};

/**
 * Read next chunk of results from DirectoryReader.
 * @protected
 */
DirectoryContents.prototype.readNextChunk = function() {
  throw 'Not implemented.';
};

/**
 * Cancel the running scan.
 */
DirectoryContents.prototype.cancelScan = function() {
  this.scanCancelled_ = true;
  cr.dispatchSimpleEvent(this, 'scan-cancelled');
};


/**
 * Called in case scan has failed. Should send the event.
 * @protected
 */
DirectoryContents.prototype.onError = function() {
  cr.dispatchSimpleEvent(this, 'scan-failed');
};

/**
 * Called in case scan has completed succesfully. Should send the event.
 * @protected
 */
DirectoryContents.prototype.lastChunkReceived = function() {
  this.allChunksFetched_ = true;
  if (!this.scanCancelled_ && this.pendingMetadataRequests_ === 0)
    cr.dispatchSimpleEvent(this, 'scan-completed');
};

/**
 * Cache necessary data before a sort happens.
 *
 * This is called by the table code before a sort happens, so that we can
 * go fetch data for the sort field that we may not have yet.
 * @param {string} field Sort field.
 * @param {function} callback Called when done.
 * @private
 */
DirectoryContents.prototype.prepareSort_ = function(field, callback) {
  this.prefetchMetadata(this.fileList_.slice(), callback);
};

/**
 * @param {Array.<Entry>} entries Files.
 * @param {function} callback Callback on done.
 */
DirectoryContents.prototype.prefetchMetadata = function(entries, callback) {
  this.context_.metadataCache.get(entries, 'filesystem', callback);
};

/**
 * @protected
 * @param {Array.<Entry>} entries File list.
 */
DirectoryContents.prototype.onNewEntries = function(entries) {
  if (this.scanCancelled_)
    return;

  var entriesFiltered = [].filter.call(entries, this.filter_);

  var onPrefetched = function() {
    this.pendingMetadataRequests_--;
    if (this.scanCancelled_)
      return;
    this.fileList_.push.apply(this.fileList_, entriesFiltered);

    if (this.pendingMetadataRequests_ === 0 && this.allChunksFetched_) {
      cr.dispatchSimpleEvent(this, 'scan-completed');
    }

    if (!this.allChunksFetched_)
      this.readNextChunk();
  };

  this.pendingMetadataRequests_++;
  this.prefetchMetadata(entriesFiltered, onPrefetched.bind(this));
};

/**
 * @param {string} name Directory name.
 * @param {function} successCallback Called on success.
 * @param {function} errorCallback On error.
 */
DirectoryContents.prototype.createDirectory = function(
    name, successCallback, errorCallback) {
  throw 'Not implemented.';
};


/**
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} entry DirectoryEntry for current directory.
 * @constructor
 * @extends {DirectoryContents}
 */
function DirectoryContentsBasic(context, entry) {
  DirectoryContents.call(this, context);
  this.entry_ = entry;
}

/**
 * Extends DirectoryContents
 */
DirectoryContentsBasic.prototype.__proto__ = DirectoryContents.prototype;

/**
 * Create the copy of the object, but without scan started.
 * @return {DirectoryContentsBasic} Object copy.
 */
DirectoryContentsBasic.prototype.clone = function() {
  return new DirectoryContentsBasic(this.context_, this.entry_);
};

/**
 * @return {string} Current path.
 */
DirectoryContentsBasic.prototype.getPath = function() {
  return this.entry_.fullPath;
};

/**
 * @return {DirectoryEntry} DirectoryEntry of the current directory.
 */
DirectoryContentsBasic.prototype.getDirectoryEntry = function() {
  return this.entry_;
};

/**
 * @return {DirectoryEntry} DirectoryEntry for the currnet entry.
 */
DirectoryContentsBasic.prototype.getLastNonSearchDirectoryEntry = function() {
  return this.entry_;
};

/**
 * Start directory scan.
 */
DirectoryContentsBasic.prototype.scan = function() {
  if (this.entry_ === DirectoryModel.fakeDriveEntry_) {
    this.lastChunkReceived();
    return;
  }

  metrics.startInterval('DirectoryScan');
  this.reader_ = this.entry_.createReader();
  this.readNextChunk();
};

/**
 * Read next chunk of results from DirectoryReader.
 * @protected
 */
DirectoryContentsBasic.prototype.readNextChunk = function() {
  this.reader_.readEntries(this.onChunkComplete_.bind(this),
                           this.onError.bind(this));
};

/**
 * @param {Array.<Entry>} entries File list.
 * @private
 */
DirectoryContentsBasic.prototype.onChunkComplete_ = function(entries) {
  if (this.scanCancelled_)
    return;

  if (entries.length == 0) {
    this.lastChunkReceived();
    this.recordMetrics_();
    return;
  }

  this.onNewEntries(entries);
};

/**
 * @private
 */
DirectoryContentsBasic.prototype.recordMetrics_ = function() {
  metrics.recordInterval('DirectoryScan');
  if (this.entry_.fullPath === RootDirectory.DOWNLOADS) {
    metrics.recordMediumCount('DownloadsCount', this.fileList_.length);
  }
};

/**
 * @param {string} name Directory name.
 * @param {function} successCallback Called on success.
 * @param {function} errorCallback On error.
 */
DirectoryContentsBasic.prototype.createDirectory = function(
    name, successCallback, errorCallback) {
  var onSuccess = function(newEntry) {
    this.prefetchMetadata([newEntry], function() {successCallback(newEntry);});
  };

  this.entry_.getDirectory(name, {create: true, exclusive: true},
                           onSuccess.bind(this), errorCallback);
};

/**
 * Delay to be used for drive search scan.
 * The goal is to reduce the number of server requests when user is typing the
 * query.
 */
DirectoryContentsDriveSearch.SCAN_DELAY = 200;

/**
 * Number of results at which we stop the search.
 * Note that max number of shown results is MAX_RESULTS + search feed size.
 */
DirectoryContentsDriveSearch.MAX_RESULTS = 999;

/**
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} dirEntry Current directory.
 * @param {DirectoryEntry} previousDirEntry DirectoryEntry that was current
 *     before the search.
 * @param {string} query Search query.
 * @constructor
 * @extends {DirectoryContents}
 */
function DirectoryContentsDriveSearch(context,
                                      dirEntry,
                                      previousDirEntry,
                                      query) {
  DirectoryContents.call(this, context);
  this.query_ = query;
  this.directoryEntry_ = dirEntry;
  this.previousDirectoryEntry_ = previousDirEntry;
  this.nextFeed_ = '';
  this.done_ = false;
  this.fetchedResultsNum_ = 0;
}

/**
 * Extends DirectoryContents.
 */
DirectoryContentsDriveSearch.prototype.__proto__ = DirectoryContents.prototype;

/**
 * Create the copy of the object, but without scan started.
 * @return {DirectoryContentsBasic} Object copy.
 */
DirectoryContentsDriveSearch.prototype.clone = function() {
  return new DirectoryContentsDriveSearch(
      this.context_, this.directoryEntry_,
      this.previousDirectoryEntry_, this.query_);
};

/**
 * @return {boolean} True if this is search results (yes).
 */
DirectoryContentsDriveSearch.prototype.isSearch = function() {
  return true;
};

/**
 * @return {DirectoryEntry} A DirectoryEntry for the top directory from which
 *     search is run (i.e. drive root).
 */
DirectoryContentsDriveSearch.prototype.getDirectoryEntry = function() {
  return this.directoryEntry_;
};

/**
 * @return {DirectoryEntry} DirectoryEntry for the directory that was current
 *     before the search.
 */
DirectoryContentsDriveSearch.prototype.getLastNonSearchDirectoryEntry =
    function() {
  return this.previousDirectoryEntry_;
};

/**
 * @return {string} The path.
 */
DirectoryContentsDriveSearch.prototype.getPath = function() {
  return this.directoryEntry_.fullPath;
};

/**
 * Start directory scan.
 */
DirectoryContentsDriveSearch.prototype.scan = function() {
  // Let's give another search a chance to cancel us before we begin.
  setTimeout(this.readNextChunk.bind(this),
             DirectoryContentsDriveSearch.SCAN_DELAY);
};

/**
 * All the results are read in one chunk, so when we try to read second chunk,
 * it means we're done.
 */
DirectoryContentsDriveSearch.prototype.readNextChunk = function() {
  if (this.scanCancelled_)
    return;

  if (this.done_) {
    this.lastChunkReceived();
    return;
  }

  var searchCallback = (function(results, nextFeed) {
    // TODO(tbarzic): Improve error handling.
    if (!results) {
      console.log('Drive search encountered an error');
      this.lastChunkReceived();
      return;
    }
    this.nextFeed_ = nextFeed;
    this.fetchedResultsNum_ += results.length;
    if (this.fetchedResultsNum_ >= DirectoryContentsDriveSearch.MAX_RESULTS)
      this.nextFeed_ = '';

    this.done_ = (this.nextFeed_ == '');

    // TODO(haruki): Use the file properties as well when we implement the UI
    // side.
    var entries = results.map(function(r) { return r.entry; });
    this.onNewEntries(entries);
  }).bind(this);

  var searchParams = {
    'query': this.query_,
    'sharedWithMe': false,  // (leave out for false)
    'nextFeed': this.nextFeed_
  };
  chrome.fileBrowserPrivate.searchDrive(searchParams, searchCallback);
};


/**
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} dirEntry Current directory.
 * @param {string} query Search query.
 * @constructor
 * @extends {DirectoryContents}
 */
function DirectoryContentsLocalSearch(context, dirEntry, query) {
  DirectoryContents.call(this, context);
  this.directoryEntry_ = dirEntry;
  this.query_ = query.toLowerCase();
}

/**
 * Extends DirectoryContents
 */
DirectoryContentsLocalSearch.prototype.__proto__ = DirectoryContents.prototype;

/**
 * Create the copy of the object, but without scan started.
 * @return {DirectoryContentsBasic} Object copy.
 */
DirectoryContentsLocalSearch.prototype.clone = function() {
  return new DirectoryContentsLocalSearch(
      this.context_, this.directoryEntry_, this.query_);
};

/**
 * @return {string} The path.
 */
DirectoryContentsLocalSearch.prototype.getPath = function() {
  return this.directoryEntry_.fullPath;
};

/**
 * @return {boolean} True if search results (drive or local).
 */
DirectoryContentsLocalSearch.prototype.isSearch = function() {
  return true;
};

/**
 * @return {DirectoryEntry} A DirectoryEntry for the top directory from which
 *     search is run.
 */
DirectoryContentsLocalSearch.prototype.getDirectoryEntry = function() {
  return this.directoryEntry_;
};

/**
 * @return {DirectoryEntry} DirectoryEntry for current directory (the search is
 *     run from the directory that was current before search).
 */
DirectoryContentsLocalSearch.prototype.getLastNonSearchDirectoryEntry =
    function() {
  return this.directoryEntry_;
};

/**
 * Start directory scan/search operation. Either 'scan-completed' or
 * 'scan-failed' event will be fired upon completion.
 */
DirectoryContentsLocalSearch.prototype.scan = function() {
  this.pendingScans_ = 0;
  this.scanDirectory_(this.directoryEntry_);
};

/**
 * Scan a directory.
 * @param {DirectoryEntry} entry A directory to scan.
 * @private
 */
DirectoryContentsLocalSearch.prototype.scanDirectory_ = function(entry) {
  this.pendingScans_++;
  var reader = entry.createReader();
  var found = [];

  var onChunkComplete = function(entries) {
    if (this.scanCancelled_)
      return;

    if (entries.length === 0) {
      if (found.length > 0)
        this.onNewEntries(found);
      this.pendingScans_--;
      if (this.pendingScans_ === 0)
        this.lastChunkReceived();
      return;
    }

    for (var i = 0; i < entries.length; i++) {
      if (entries[i].name.toLowerCase().indexOf(this.query_) != -1) {
        found.push(entries[i]);
      }

      if (entries[i].isDirectory)
        this.scanDirectory_(entries[i]);
    }

    getNextChunk();
  }.bind(this);

  var getNextChunk = function() {
    reader.readEntries(onChunkComplete, this.onError.bind(this));
  }.bind(this);

  getNextChunk();
};

/**
 * We get results for each directory in one go in scanDirectory_.
 */
DirectoryContentsLocalSearch.prototype.readNextChunk = function() {
};

/**
 * DirectoryContents to list Drive files available offline. The search is done
 * by traversing the directory tree under "My Drive" and filtering them using
 * the availableOffline property in 'drive' metadata.
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} driveDirEntry Directory for actual Drive. Traversal
 *     starts from this Entry. Should be null if underlying Drive is not
 *     available.
 * @param {DirectoryEntry} fakeOfflineDirEntry Fake directory representing
 *     the set of offline files. This serves as a top directory for this search.
 * @param {string} query Search query to filter the files.
 * @constructor
 * @extends {DirectoryContents}
 */
function DirectoryContentsDriveOffline(context,
                                       driveDirEntry,
                                       fakeOfflineDirEntry,
                                       query) {
  DirectoryContents.call(this, context);
  this.driveDirEntry_ = driveDirEntry;
  this.fakeOfflineDirEntry_ = fakeOfflineDirEntry;
  this.query_ = query;
}

/**
 * Extends DirectoryContents.
 */
DirectoryContentsDriveOffline.prototype.__proto__ = DirectoryContents.prototype;

/**
 * Creates a copy of the object, but without scan started.
 * @return {DirectoryContents} Object copy.
 */
DirectoryContentsDriveOffline.prototype.clone = function() {
  return new DirectoryContentsDriveOffline(
      this.context_, this.directoryEntry_, this.fakeOfflineDirEntry_,
      this.query_);
};

/**
 * @return {boolean} True if this is search results (yes).
 */
DirectoryContentsDriveOffline.prototype.isSearch = function() {
  return true;
};

/**
 * @return {DirectoryEntry} An Entry representing the current contents
 *     (i.e. fake root for "Offline").
 */
DirectoryContentsDriveOffline.prototype.getDirectoryEntry = function() {
  return this.fakeOfflineDirEntry_;
};

/**
 * @return {DirectoryEntry} DirectoryEntry for the directory that was current
 *     before the search.
 */
DirectoryContentsDriveOffline.prototype.getLastNonSearchDirectoryEntry =
    function() {
  return this.driveDirEntry_;
};

/**
 * @return {string} The path.
 */
DirectoryContentsDriveOffline.prototype.getPath = function() {
  return this.fakeOfflineDirEntry_.fullPath;
};

/**
 * Starts directory scan.
 */
DirectoryContentsDriveOffline.prototype.scan = function() {
  this.pendingScans_ = 0;
  if (this.driveDirEntry_) {
    this.scanDirectory_(this.driveDirEntry_);
  } else {
    // Show nothing when Drive is not available.
    this.lastChunkReceived();
  }
};

/**
 * Scans a directory.
 * @param {DirectoryEntry} entry A directory to scan.
 * @private
 */
DirectoryContentsDriveOffline.prototype.scanDirectory_ = function(entry) {
  this.pendingScans_++;
  var reader = entry.createReader();
  var candidates = [];

  var getNextChunk = function() {
    reader.readEntries(onChunkComplete, this.onError.bind(this));
  }.bind(this);

  var onChunkComplete = function(entries) {
    if (this.scanCancelled_)
      return;

    if (entries.length === 0) {
      if (candidates.length > 0) {
        // Retrieve 'drive' metadata and check if the file is available offline.
        this.context_.metadataCache.get(
            candidates, 'drive',
            function(properties) {
              var results = [];
              for (var i = 0; i < properties.length; i++) {
                if (properties[i].availableOffline)
                  results.push(candidates[i]);
              }
              if (results.length > 0)
                this.onNewEntries(results);
            }.bind(this));
      }

      this.pendingScans_--;
      if (this.pendingScans_ === 0)
        this.lastChunkReceived();
      return;
    }

    for (var i = 0; i < entries.length; i++) {
      var resultEntry = entries[i];

      // Will check metadata for files with names matching the query.
      // When the query is empty, check all the files.
      if (resultEntry.isFile &&
          (!this.query_ ||
           resultEntry.name.toLowerCase().indexOf(this.query_) != -1)) {
        candidates.push(entries[i]);
      } else if (resultEntry.isDirectory) {
        this.scanDirectory_(entries[i]);
      }
    }

    getNextChunk();
  }.bind(this);

  getNextChunk();
};

/**
 * Everything is done in scanDirectory_().
 */
DirectoryContentsDriveOffline.prototype.readNextChunk = function() {
};
