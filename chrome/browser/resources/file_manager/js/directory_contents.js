// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Scanner of the entries.
 * @constructor
 */
function ContentScanner() {
  this.cancelled_ = false;
}

/**
 * Starts to scan the entries. For example, starts to read the entries in a
 * directory, or starts to search with some query on a file system.
 * Derived classes must override this method.
 *
 * @param {function(Array.<Entry>)} entriesCallback Called when some chunk of
 *     entries are read. This can be called a couple of times until the
 *     completion.
 * @param {function()} successCallback Called when the scan is completed
 *     successfully.
 * @param {function(FileError)} errorCallback Called an error occurs.
 */
ContentScanner.prototype.scan = function(
    entriesCallback, successCallback, errorCallback) {
};

/**
 * Request cancelling of the running scan. When the cancelling is done,
 * an error will be reported from errorCallback passed to scan().
 */
ContentScanner.prototype.cancel = function() {
  this.cancelled_ = true;
};

/**
 * Scanner of the entries in a directory.
 * @param {DirectoryEntry} entry The directory to be read.
 * @constructor
 * @extends {ContentScanner}
 */
function DirectoryContentScanner(entry) {
  ContentScanner.call(this);
  this.entry_ = entry;
}

/**
 * Extends ContentScanner.
 */
DirectoryContentScanner.prototype.__proto__ = ContentScanner.prototype;

/**
 * Starts to read the entries in the directory.
 * @param {function(Array.<Entry>)} entriesCallback Called when some chunk of
 *     entries are read. This can be called a couple of times until the
 *     completion.
 * @param {function()} successCallback Called when the scan is completed
 *     successfully.
 * @param {function(FileError)} errorCallback Called an error occurs.
 * @override
 */
DirectoryContentScanner.prototype.scan = function(
    entriesCallback, successCallback, errorCallback) {
  if (!this.entry_ || this.entry_ === DirectoryModel.fakeDriveEntry_) {
    // If entry is not specified or a fake, we cannot read it.
    errorCallback(util.createFileError(FileError.INVALID_MODIFICATION_ERR));
    return;
  }

  metrics.startInterval('DirectoryScan');
  var reader = this.entry_.createReader();
  var readEntries = function() {
    reader.readEntries(
        function(entries) {
          if (this.cancelled_) {
            errorCallback(util.createFileError(FileError.ABORT_ERR));
            return;
          }

          if (entries.length === 0) {
            // All entries are read.
            metrics.recordInterval('DirectoryScan');
            successCallback();
            return;
          }

          entriesCallback(entries);
          readEntries();
        }.bind(this),
        errorCallback);
  }.bind(this);
  readEntries();
};

/**
 * Scanner of the entries for the search results on Drive File System.
 * @param {string} query The query string.
 * @constructor
 * @extends {ContentScanner}
 */
function DriveSearchContentScanner(query) {
  ContentScanner.call(this);
  this.query_ = query;
}

/**
 * Extends ContentScanner.
 */
DriveSearchContentScanner.prototype.__proto__ = ContentScanner.prototype;

/**
 * Delay in milliseconds to be used for drive search scan, in order to reduce
 * the number of server requests while user is typing the query.
 * @type {number}
 * @private
 * @const
 */
DriveSearchContentScanner.SCAN_DELAY_ = 200;

/**
 * Maximum number of results which is shown on the search.
 * @type {number}
 * @private
 * @const
 */
DriveSearchContentScanner.MAX_RESULTS_ = 100;

/**
 * Starts to search on Drive File System.
 * @param {function(Array.<Entry>)} entriesCallback Called when some chunk of
 *     entries are read. This can be called a couple of times until the
 *     completion.
 * @param {function()} successCallback Called when the scan is completed
 *     successfully.
 * @param {function(FileError)} errorCallback Called an error occurs.
 * @override
 */
DriveSearchContentScanner.prototype.scan = function(
    entriesCallback, successCallback, errorCallback) {
  var numReadEntries = 0;
  var readEntries = function(nextFeed) {
    chrome.fileBrowserPrivate.searchDrive(
        {query: this.query_, nextFeed: nextFeed},
        function(entries, nextFeed) {
          if (this.cancelled_) {
            errorCallback(util.createFileError(FileError.ABORT_ERR));
            return;
          }

          // TODO(tbarzic): Improve error handling.
          if (!entries) {
            console.error('Drive search encountered an error.');
            errorCallback(util.createFileError(
                FileError.INVALID_MODIFICATION_ERR));
            return;
          }

          var numRemainingEntries =
              DriveSearchContentScanner.MAX_RESULTS_ - numReadEntries;
          if (entries.length >= numRemainingEntries) {
            // The limit is hit, so quit the scan here.
            entries = entries.slice(0, numRemainingEntries);
            nextFeed = '';
          }

          numReadEntries += entries.length;
          if (entries.length > 0)
            entriesCallback(entries);

          if (nextFeed === '')
            successCallback();
          else
            readEntries(nextFeed);
        }.bind(this));
  }.bind(this);

  // Let's give another search a chance to cancel us before we begin.
  setTimeout(
      function() {
        // Check cancelled state before read the entries.
        if (this.cancelled_) {
          errorCallback(util.createFileError(FileError.ABORT_ERR));
          return;
        }
        readEntries('');
      }.bind(this),
      DriveSearchContentScanner.SCAN_DELAY_);
};

/**
 * Scanner of the entries of the file name search on the directory tree, whose
 * root is entry.
 * @param {DirectoryEntry} entry The root of the search target directory tree.
 * @param {string} query The query of the search.
 * @constructor
 * @extends {ContentScanner}
 */
function LocalSearchContentScanner(entry, query) {
  ContentScanner.call(this);
  this.entry_ = entry;
  this.query_ = query.toLowerCase();
}

/**
 * Extedns ContentScanner.
 */
LocalSearchContentScanner.prototype.__proto__ = ContentScanner.prototype;

/**
 * Starts the file name search.
 * @param {function(Array.<Entry>)} entriesCallback Called when some chunk of
 *     entries are read. This can be called a couple of times until the
 *     completion.
 * @param {function()} successCallback Called when the scan is completed
 *     successfully.
 * @param {function(FileError)} errorCallback Called an error occurs.
 * @override
 */
LocalSearchContentScanner.prototype.scan = function(
    entriesCallback, successCallback, errorCallback) {
  var numRunningTasks = 0;
  var error = null;
  var maybeRunCallback = function() {
    if (numRunningTasks === 0) {
      if (this.cancelled_)
        errorCallback(util.createFileError(FileError.ABORT_ERR));
      else if (error)
        errorCallback(error);
      else
        successCallback();
    }
  }.bind(this);

  var processEntry = function(entry) {
    numRunningTasks++;
    var onError = function(fileError) {
      if (!error)
        error = fileError;
      numRunningTasks--;
      maybeRunCallback();
    };

    var onSuccess = function(entries) {
      if (this.cancelled_ || error || entries.length === 0) {
        numRunningTasks--;
        maybeRunCallback();
        return;
      }

      // Filters by the query, and if found, run entriesCallback.
      var foundEntries = entries.filter(function(entry) {
        return entry.name.toLowerCase().indexOf(this.query_) >= 0;
      }.bind(this));
      if (foundEntries.length > 0)
        entriesCallback(foundEntries);

      // Start to process sub directories.
      for (var i = 0; i < entries.length; i++) {
        if (entries[i].isDirectory)
          processEntry(entries[i]);
      }

      // Read remaining entries.
      reader.readEntries(onSuccess, onError);
    }.bind(this);

    var reader = entry.createReader();
    reader.readEntries(onSuccess, onError);
  }.bind(this);

  processEntry(this.entry_);
};

/**
 * Scanner of the entries for the metadata seaerch on Drive File System.
 * @param {string} query The query of the search.
 * @param {DriveMetadataSearchContentScanner.SearchType} searchType The option
 *     of the search.
 * @constructor
 * @extends {ContentScanner}
 */
function DriveMetadataSearchContentScanner(query, searchType) {
  ContentScanner.call(this);
  this.query_ = query;
  this.searchType_ = searchType;
}

/**
 * Extends ContentScanner.
 */
DriveMetadataSearchContentScanner.prototype.__proto__ =
    ContentScanner.prototype;

/**
 * The search types on the Drive File System.
 * @enum {string}
 */
DriveMetadataSearchContentScanner.SearchType = Object.freeze({
  SEARCH_ALL: 'ALL',
  SEARCH_SHARED_WITH_ME: 'SHARED_WITH_ME',
  SEARCH_RECENT_FILES: 'EXCLUDE_DIRECTORIES',
  SEARCH_OFFLINE: 'OFFLINE'
});

/**
 * Starts to metadata-search on Drive File System.
 * @param {function(Array.<Entry>)} entriesCallback Called when some chunk of
 *     entries are read. This can be called a couple of times until the
 *     completion.
 * @param {function()} successCallback Called when the scan is completed
 *     successfully.
 * @param {function(FileError)} errorCallback Called an error occurs.
 * @override
 */
DriveMetadataSearchContentScanner.prototype.scan = function(
    entriesCallback, successCallback, errorCallback) {
  chrome.fileBrowserPrivate.searchDriveMetadata(
      {query: this.query_, types: this.searchType_, maxResults: 500},
      function(results) {
        if (this.cancelled_) {
          errorCallback(util.createFileError(FileError.ABORT_ERR));
          return;
        }

        if (!results) {
          console.error('Drive search encountered an error.');
          errorCallback(util.createFileError(
              FileError.INVALID_MODIFICATION_ERR));
          return;
        }

        var entries = results.map(function(result) { return result.entry; });
        if (entries.length > 0)
          entriesCallback(entries);
        successCallback();
      }.bind(this));
};

/**
 * This class manages filters and determines a file should be shown or not.
 * When filters are changed, a 'changed' event is fired.
 *
 * @param {MetadataCache} metadataCache Metadata cache service.
 * @param {boolean} showHidden If files starting with '.' are shown.
 * @constructor
 * @extends {cr.EventTarget}
 */
function FileFilter(metadataCache, showHidden) {
  /**
   * @type {MetadataCache}
   * @private
   */
  this.metadataCache_ = metadataCache;

  /**
   * @type Object.<string, Function>
   * @private
   */
  this.filters_ = {};
  this.setFilterHidden(!showHidden);

  // Do not show entries marked as 'deleted'.
  this.addFilter('deleted', function(entry) {
    var internal = this.metadataCache_.getCached(entry, 'internal');
    return !(internal && internal.deleted);
  }.bind(this));
}

/*
 * FileFilter extends cr.EventTarget.
 */
FileFilter.prototype = {__proto__: cr.EventTarget.prototype};

/**
 * @param {string} name Filter identifier.
 * @param {function(Entry)} callback A filter — a function receiving an Entry,
 *     and returning bool.
 */
FileFilter.prototype.addFilter = function(name, callback) {
  this.filters_[name] = callback;
  cr.dispatchSimpleEvent(this, 'changed');
};

/**
 * @param {string} name Filter identifier.
 */
FileFilter.prototype.removeFilter = function(name) {
  delete this.filters_[name];
  cr.dispatchSimpleEvent(this, 'changed');
};

/**
 * @param {boolean} value If do not show hidden files.
 */
FileFilter.prototype.setFilterHidden = function(value) {
  if (value) {
    this.addFilter(
        'hidden',
        function(entry) { return entry.name.substr(0, 1) !== '.'; }
    );
  } else {
    this.removeFilter('hidden');
  }
};

/**
 * @return {boolean} If the files with names starting with "." are not shown.
 */
FileFilter.prototype.isFilterHiddenOn = function() {
  return 'hidden' in this.filters_;
};

/**
 * @param {Entry} entry File entry.
 * @return {boolean} True if the file should be shown, false otherwise.
 */
FileFilter.prototype.filter = function(entry) {
  for (var name in this.filters_) {
    if (!this.filters_[name](entry))
      return false;
  }
  return true;
};

/**
 * A context of DirectoryContents.
 * TODO(yoshiki): remove this. crbug.com/224869.
 *
 * @param {FileFilter} fileFilter The file-filter context.
 * @param {MetadataCache} metadataCache Metadata cache service.
 * @constructor
 */
function FileListContext(fileFilter, metadataCache) {
  /**
   * @type {cr.ui.ArrayDataModel}
   */
  this.fileList = new cr.ui.ArrayDataModel([]);

  /**
   * @type {MetadataCache}
   */
  this.metadataCache = metadataCache;

  /**
   * @type {FileFilter}
   */
  this.fileFilter = fileFilter;
}

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
  this.prefetchMetadataQueue_ = new AsyncUtil.Queue();
  this.scanCancelled_ = false;
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
 * @return {boolean} If the scan is active.
 */
DirectoryContents.prototype.isScanning = function() {
  return this.scanner_ || this.prefetchMetadataQueue_.isRunning();
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
 * Cancels the running scan.
 */
DirectoryContents.prototype.cancelScan = function() {
  if (this.scanCancelled_)
    return;
  this.scanCancelled_ = true;
  if (this.scanner_)
    this.scanner_.cancel();

  this.prefetchMetadataQueue_.cancel();
  cr.dispatchSimpleEvent(this, 'scan-cancelled');
};

/**
 * Called when the scanning by scanner_ is done.
 * @protected
 */
DirectoryContents.prototype.onScanCompleted = function() {
  this.scanner_ = null;
  if (this.scanCancelled_)
    return;

  this.prefetchMetadataQueue_.run(function(callback) {
    cr.dispatchSimpleEvent(this, 'scan-completed');
    if (!this.isSearch() &&
        this.getDirectoryEntry().fullPath === RootDirectory.DOWNLOADS)
      metrics.recordMediumCount('DownloadsCount', this.fileList_.length);
    callback();
  }.bind(this));
};

/**
 * Called in case scan has failed. Should send the event.
 * @protected
 */
DirectoryContents.prototype.onScanError = function() {
  this.scanner_ = null;
  if (this.scanCancelled_)
    return;

  this.prefetchMetadataQueue_.run(function(callback) {
    cr.dispatchSimpleEvent(this, 'scan-failed');
    callback();
  }.bind(this));
};

/**
 * Called when some chunk of entries are read by scanner.
 * @param {Array.<Entry>} entries The list of the scanned entries.
 * @protected
 */
DirectoryContents.prototype.onNewEntries = function(entries) {
  if (this.scanCancelled_)
    return;

  var entriesFiltered = [].filter.call(
      entries, this.context_.fileFilter.filter.bind(this.context_.fileFilter));

  // Because the prefetchMetadata can be slow, throttling by splitting entries
  // into smaller chunks to reduce UI latency.
  // TODO(hidehiko,mtomasz): This should be handled in MetadataCache.
  var MAX_CHUNK_SIZE = 50;
  for (var i = 0; i < entriesFiltered.length; i += MAX_CHUNK_SIZE) {
    var chunk = entriesFiltered.slice(i, i + MAX_CHUNK_SIZE);
    this.prefetchMetadataQueue_.run(function(chunk, callback) {
      this.prefetchMetadata(chunk, function() {
        if (this.scanCancelled_) {
          // Do nothing if the scanning is cancelled.
          callback();
          return;
        }

        this.fileList_.push.apply(this.fileList_, chunk);
        cr.dispatchSimpleEvent(this, 'scan-updated');
        callback();
      }.bind(this));
    }.bind(this, chunk));
  }
};

/**
 * Cache necessary data before a sort happens.
 *
 * This is called by the table code before a sort happens, so that we can
 * go fetch data for the sort field that we may not have yet.
 * @param {string} field Sort field.
 * @param {function(Object)} callback Called when done.
 * @private
 */
DirectoryContents.prototype.prepareSort_ = function(field, callback) {
  this.prefetchMetadata(this.fileList_.slice(), callback);
};

/**
 * @param {Array.<Entry>} entries Files.
 * @param {function(Object)} callback Callback on done.
 */
DirectoryContents.prototype.prefetchMetadata = function(entries, callback) {
  this.context_.metadataCache.get(entries, 'filesystem', callback);
};

/**
 * @param {Array.<Entry>} entries Files.
 * @param {function(Object)} callback Callback on done.
 */
DirectoryContents.prototype.reloadMetadata = function(entries, callback) {
  this.context_.metadataCache.clear(entries, '*');
  this.context_.metadataCache.get(entries, 'filesystem', callback);
};

/**
 * @param {string} name Directory name.
 * @param {function(DirectoryEntry)} successCallback Called on success.
 * @param {function(FileError)} errorCallback On error.
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
  // TODO(hidehiko,mtomasz): this scan method must be called at most once.
  // Remove such a limitation.
  this.scanner_ = new DirectoryContentScanner(this.entry_);
  this.scanner_.scan(this.onNewEntries.bind(this),
                     this.onScanCompleted.bind(this),
                     this.onScanError.bind(this));
};

/**
 * @param {string} name Directory name.
 * @param {function(Entry)} successCallback Called on success.
 * @param {function(FileError)} errorCallback On error.
 */
DirectoryContentsBasic.prototype.createDirectory = function(
    name, successCallback, errorCallback) {
  // TODO(hidehiko): createDirectory should not be the part of
  // DirectoryContent.
  if (!this.entry_) {
    errorCallback(util.createFileError(FileError.INVALID_MODIFICATION_ERR));
    return;
  }

  var onSuccess = function(newEntry) {
    this.reloadMetadata([newEntry], function() {
      successCallback(newEntry);
    });
  };

  this.entry_.getDirectory(name, {create: true, exclusive: true},
                           onSuccess.bind(this), errorCallback);
};

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
  this.directoryEntry_ = dirEntry;
  this.previousDirectoryEntry_ = previousDirEntry;
  this.query_ = query;
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
 * Start directory scan.
 */
DirectoryContentsDriveSearch.prototype.scan = function() {
  this.scanner_ = new DriveSearchContentScanner(this.query_);
  this.scanner_.scan(this.onNewEntries.bind(this),
                     this.onScanCompleted.bind(this),
                     this.onScanError.bind(this));
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
  this.query_ = query;
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
  this.scanner_ =
      new LocalSearchContentScanner(this.directoryEntry_, this.query_);
  this.scanner_.scan(this.onNewEntries.bind(this),
                     this.onScanCompleted.bind(this),
                     this.onScanError.bind(this));
};

/**
 * DirectoryContents to list Drive files using searchDriveMetadata().
 *
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} driveDirEntry Directory for actual Drive.
 * @param {DirectoryEntry} fakeDirEntry Fake directory representing the set of
 *     result files. This serves as a top directory for this search.
 * @param {string} query Search query to filter the files.
 * @param {DriveMetadataSearchContentScanner.SearchType} searchType
 *     Type of search. searchDriveMetadata will restricts the entries based on
 *     the given search type.
 * @constructor
 * @extends {DirectoryContents}
 */
function DirectoryContentsDriveSearchMetadata(context,
                                              driveDirEntry,
                                              fakeDirEntry,
                                              query,
                                              searchType) {
  DirectoryContents.call(this, context);
  this.driveDirEntry_ = driveDirEntry;
  this.fakeDirEntry_ = fakeDirEntry;
  this.query_ = query;
  this.searchType_ = searchType;
}

/**
 * Creates a copy of the object, but without scan started.
 * @return {DirectoryContents} Object copy.
 */
DirectoryContentsDriveSearchMetadata.prototype.clone = function() {
  return new DirectoryContentsDriveSearchMetadata(
      this.context_, this.driveDirEntry_, this.fakeDirEntry_, this.query_,
      this.searchType_);
};

/**
 * Extends DirectoryContents.
 */
DirectoryContentsDriveSearchMetadata.prototype.__proto__ =
    DirectoryContents.prototype;

/**
 * @return {boolean} True if this is search results (yes).
 */
DirectoryContentsDriveSearchMetadata.prototype.isSearch = function() {
  return true;
};

/**
 * @return {DirectoryEntry} An Entry representing the current contents
 *     (i.e. fake root for "Shared with me").
 */
DirectoryContentsDriveSearchMetadata.prototype.getDirectoryEntry = function() {
  return this.fakeDirEntry_;
};

/**
 * @return {DirectoryEntry} DirectoryEntry for the directory that was current
 *     before the search.
 */
DirectoryContentsDriveSearchMetadata.prototype.getLastNonSearchDirectoryEntry =
    function() {
  return this.driveDirEntry_;
};

/**
 * Start directory scan/search operation. Either 'scan-completed' or
 * 'scan-failed' event will be fired upon completion.
 */
DirectoryContentsDriveSearchMetadata.prototype.scan = function() {
  this.scanner_ =
      new DriveMetadataSearchContentScanner(this.query_, this.searchType_);
  this.scanner_.scan(this.onNewEntries.bind(this),
                     this.onScanCompleted.bind(this),
                     this.onScanError.bind(this));
};
