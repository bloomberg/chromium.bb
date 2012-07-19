// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Type of a root directory.
 * @enum
 */
var RootType = {
  DOWNLOADS: 'downloads',
  ARCHIVE: 'archive',
  REMOVABLE: 'removable',
  GDATA: 'gdata'
};

/**
 * Top directory for each root type.
 * @type {Object.<RootType,string>}
 */
var RootDirectory = {
  DOWNLOADS: '/Downloads',
  ARCHIVE: '/archive',
  REMOVABLE: '/removable',
  GDATA: '/drive'
};

var PathUtil = {};

/**
 * @param {string} path Path starting with '/'.
 * @return {string} Root directory (starting with '/').
 */
PathUtil.getRootDirectory = function(path) {
  var i = path.indexOf('/', 1);
  return i === -1 ? path.substring(0) : path.substring(0, i);
};

/**
 * @param {string} path Any unix-style path (may start or not start from root).
 * @return {Array.<string>} path components
 */
PathUtil.split = function(path) {
  var fromRoot = false;
  if (path[0] === '/') {
    fromRoot = true;
    path = path.substring(1);
  }

  var components = path.split('/');
  if (fromRoot)
    components[0] = '/' + components[0];
  return components;
};

/**
 * Join path components into a single path. Can be called either with a list of
 * components as arguments, or with an array of components as the only argument.
 *
 * Examples:
 * Path.join('abc', 'def') -> 'abc/def'
 * Path.join('/', 'abc', 'def/ghi') -> '/abc/def/ghi'
 * Path.join(['/abc/def', 'ghi']) -> '/abc/def/ghi'
 *
 * @return {string} Resulting path.
 */
PathUtil.join = function() {
  var components;

  if (arguments.length === 1 && typeof(arguments[0]) === 'object') {
    components = arguments[0];
  } else {
    components = arguments;
  }

  var path = '';
  for (var i = 0; i < components.length; i++) {
    if (components[i][0] === '/') {
      path = components[i];
      continue;
    }
    if (path.length === 0 || path[path.length - 1] !== '/')
      path += '/';
    path += components[i];
  }
  return path;
};

/**
 * @param {string} path Path starting with '/'.
 * @return {RootType} RootType.DOWNLOADS, RootType.GDATA etc.
 */
PathUtil.getRootType = function(path) {
  var rootDir = PathUtil.getRootDirectory(path);
  for (var type in RootDirectory) {
    if (rootDir === RootDirectory[type])
      return RootType[type];
  }
};

/**
 * @param {string} path Any path.
 * @return {string} The root path.
 */
PathUtil.getRootPath = function(path) {
  var type = PathUtil.getRootType(path);

  if (type == RootType.DOWNLOADS || type == RootType.GDATA)
    return PathUtil.getRootDirectory(path);

  if (type == RootType.ARCHIVE || type == RootType.REMOVABLE) {
    var components = PathUtil.split(path);
    if (components.length > 1) {
      return PathUtil.join(components[0], components[1]);
    } else {
      return components[0];
    }
  }

  return '/';
};

/**
 * @param {string} path A path.
 * @return {boolean} True if it is a path to the root.
 */
PathUtil.isRootPath = function(path) {
  return PathUtil.getRootPath(path) === path;
};


/**
 * @constructor
 * @param {MetadataCache} metadataCache Metadata cache service.
 * @param {cr.ui.ArrayDataModel} fileList The file list.
 * @param {boolean} showHidden If files starting with '.' are shown.
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
}

/**
 * @param {string} name Filter identifier.
 * @param {Function(Entry)} callback A filter — a function receiving an Entry,
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
 * @param {bool} value If do not show hidden files.
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
 * @return {bool} True if the file should be shown, false otherwise.
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
 * directory contents shown: basic directory, gdata search results, local search
 * results.
 * @constructor
 * @param {FileListContext} context The file list context.
 */
function DirectoryContents(context) {
  this.context_ = context;
  this.fileList_ = context.fileList;
  this.scanCompletedCallback_ = null;
  this.scanFailedCallback_ = null;
  this.scanCancelled_ = false;
  this.filter_ = context.filter.bind(context);

  this.fileList_.prepareSort = this.prepareSort_.bind(this);
}

/**
 * DirectoryModel extends cr.EventTarget.
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
  console.log((new Error()).stack);
  throw 'Not implemented.';
};

/**
 * @return {boolean} True if search results (gdata or local).
 */
DirectoryContents.prototype.isSearch = function() {
  return false;
};

/**
 * @return {DirectoryEntry} A DirectoryEntry for current directory. In case of
 *     search -- the top directory from which search is run
 */
DirectoryContents.prototype.getDirectoryEntry = function() {
  throw 'Not implemented.';
};

/**
 * @param {Entry} entry File entry for a file in current DC results.
 * @return {string} Display name.
 */
DirectoryContents.prototype.getDisplayName = function(entry) {
  return entry.name;
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
DirectoryContents.prototype.onCompleted = function() {
  cr.dispatchSimpleEvent(this, 'scan-completed');
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
    if (this.scanCancelled_)
      return;
    this.fileList_.push.apply(this.fileList_, entriesFiltered);
    this.readNextChunk();
  };

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
 * @constructor
 * @extends {DirectoryContents}
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} entry DirectoryEntry for current directory.
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
 * @return {DirectoryEntry?} DirectoryEntry of the current directory.
 */
DirectoryContentsBasic.prototype.getDirectoryEntry = function() {
  return this.entry_;
};

/**
 * Start directory scan.
 */
DirectoryContentsBasic.prototype.scan = function() {
  if (this.entry_ === DirectoryModel.fakeGDataEntry_)
    return;

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
 * @private
 * @param {Array.<Entry>} entries File list.
 */
DirectoryContentsBasic.prototype.onChunkComplete_ = function(entries) {
  if (this.scanCancelled_)
    return;

  if (entries.length == 0) {
    this.onCompleted();
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
  }

  this.entry_.getDirectory(name, {create: true, exclusive: true},
                           onSuccess.bind(this), errorCallback);
};


/**
 * @constructor
 * @extends {DirectoryContents}
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} dirEntry Current directory.
 * @param {string} query Search query.
 */
function DirectoryContentsGDataSearch(context, dirEntry, query) {
  DirectoryContents.call(this, context);
  this.query_ = query;
  this.directoryEntry_ = dirEntry;
}

/**
 * Extends DirectoryContents.
 */
DirectoryContentsGDataSearch.prototype.__proto__ = DirectoryContents.prototype;

/**
 * Create the copy of the object, but without scan started.
 * @return {DirectoryContentsBasic} Object copy.
 */
DirectoryContentsGDataSearch.prototype.clone = function() {
  return new DirectoryContentsGDataSearch(
      this.context_, this.directoryEntry_, this.query_);
};

/**
 * @return {boolean} True if this is search results (yes).
 */
DirectoryContentsGDataSearch.prototype.isSearch = function() {
  return true;
};

/**
 * @return {DirectoryEntry} DirectoryEntry for current directory.
 */
DirectoryContentsGDataSearch.prototype.getDirectoryEntry = function() {
  return this.directoryEntry_;
};

/**
 * @return {string} The path.
 */
DirectoryContentsGDataSearch.prototype.getPath = function() {
  return this.directoryEntry_.fullPath;
};

/**
 * Start directory scan.
 */
DirectoryContentsGDataSearch.prototype.scan = function() {
  chrome.fileBrowserPrivate.searchGData(this.query_,
                                        this.onNewEntries.bind(this));
};

/**
 * Empty because all the results are read in one chunk.
 */
DirectoryContentsGDataSearch.prototype.readNextChunk = function() {
};


/**
 * @constructor
 * @extends {DirectoryContents}
 * @param {FileListContext} context File list context.
 * @param {DirectoryEntry} dirEntry Current directory.
 * @param {string} query Search query.
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
 * @return {boolean} True if search results (gdata or local).
 */
DirectoryContentsLocalSearch.prototype.isSearch = function() {
  return true;
};

/**
 * @return {DirectoryEntry} A DirectoryEntry for current directory. In case of
 *     search -- the top directory from which search is run
 */
DirectoryContentsLocalSearch.prototype.getDirectoryEntry = function() {
  return this.directoryEntry_;
};

/**
 * @param {Entry} entry File entry for a file in current DC results.
 * @return {string} Display name.
 */
DirectoryContentsLocalSearch.prototype.getDisplayName = function(entry) {
  return entry.name;
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

  var self = this;

  var onChunkComplete = function(entries) {
    if (self.scanCancelled_)
      return;

    if (entries.length === 0) {
      if (found.length > 0)
        self.onNewEntries(found);
      self.pendingScans_--;

      if (self.pendingScans_ === 0)
        self.onCompleted();

      return;
    }

    for (var i = 0; i < entries.length; i++) {
      if (entries[i].name.toLowerCase().indexOf(self.query_) != -1) {
        found.push(entries[i]);
      }

      if (entries[i].isDirectory)
        self.scanDirectory_(entries[i]);
    }

    getNextChunk();
  };

  var getNextChunk = function() {
    reader.readEntries(onChunkComplete, self.onError.bind(self));
  };

  getNextChunk();
};
