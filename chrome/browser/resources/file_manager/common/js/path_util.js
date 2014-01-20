// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Type of a root directory.
 * @enum {string}
 * @const
 */
var RootType = Object.freeze({
  // Root of local directory.
  DOWNLOADS: 'downloads',

  // Root of mounted archive file.
  ARCHIVE: 'archive',

  // Root of removal volume.
  REMOVABLE: 'removable',

  // Root of drive directory.
  DRIVE: 'drive',

  // Root for privet storage volume.
  CLOUD_DEVICE: 'cloud_device',

  // Root for entries that is not located under RootType.DRIVE. e.g. shared
  // files.
  DRIVE_OTHER: 'drive_other',

  // Fake root for offline available files on the drive.
  DRIVE_OFFLINE: 'drive_offline',

  // Fake root for shared files on the drive.
  DRIVE_SHARED_WITH_ME: 'drive_shared_with_me',

  // Fake root for recent files on the drive.
  DRIVE_RECENT: 'drive_recent'
});

/**
 * Top directory for each root type.
 * TODO(mtomasz): Deprecated. Remove this.
 * @enum {string}
 * @const
 */
var RootDirectory = Object.freeze({
  DOWNLOADS: '/Downloads',
  ARCHIVE: '/archive',
  REMOVABLE: '/removable',
  DRIVE: '/drive',
  CLOUD_DEVICE: '/privet',
  DRIVE_OFFLINE: '/drive_offline',  // A fake root. Not the actual filesystem.
  DRIVE_SHARED_WITH_ME: '/drive_shared_with_me',  // A fake root.
  DRIVE_RECENT: '/drive_recent'  // A fake root.
});

var PathUtil = {};

/**
 * The default mount point.
 * TODO(mtomasz): Deprecated. Use the volume manager instead.
 * @type {string}
 * @const
 */
PathUtil.DEFAULT_MOUNT_POINT = '/Downloads';

/**
 * Checks if the given path represents a special search. Fake entries in
 * RootDirectory correspond to special searches.
 * @param {string} path Path to check.
 * @return {boolean} True if the given path represents a special search.
 */
PathUtil.isSpecialSearchRoot = function(path) {
  var type = PathUtil.getRootType(path);
  return type == RootType.DRIVE_OFFLINE ||
      type == RootType.DRIVE_SHARED_WITH_ME ||
      type == RootType.DRIVE_RECENT;
};

/**
 * Checks |path| and return true if it is under Google Drive or a special
 * search root which represents a special search from Google Drive.
 * @param {string} path Path to check.
 * @return {boolean} True if the given path represents a Drive based path.
 */
PathUtil.isDriveBasedPath = function(path) {
  var rootType = PathUtil.getRootType(path);
  return rootType === RootType.DRIVE ||
      rootType === RootType.DRIVE_SHARED_WITH_ME ||
      rootType === RootType.DRIVE_RECENT ||
      rootType === RootType.DRIVE_OFFLINE;
};

/**
 * @param {string} path Path starting with '/'.
 * @return {string} Top directory (starting with '/').
 */
PathUtil.getTopDirectory = function(path) {
  var i = path.indexOf('/', 1);
  return i === -1 ? path : path.substring(0, i);
};

/**
 * Obtains the parent path of the specified path.
 * @param {string} path Path string.
 * @return {string} Parent path.
 */
PathUtil.getParentDirectory = function(path) {
  if (path[path.length - 1] == '/')
    return PathUtil.getParentDirectory(path.substring(0, path.length - 1));
  var index = path.lastIndexOf('/');
  if (index == 0)
    return '/';
  else if (index == -1)
    return '.';
  return path.substring(0, index);
};

/**
 * @param {string} path Any unix-style path (may start or not start from root).
 * @return {Array.<string>} Path components.
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
 * Returns a directory part of the given |path|. In other words, the path
 * without its base name.
 *
 * Examples:
 * PathUtil.dirname('abc') -> ''
 * PathUtil.dirname('a/b') -> 'a'
 * PathUtil.dirname('a/b/') -> 'a/b'
 * PathUtil.dirname('a/b/c') -> 'a/b'
 * PathUtil.dirname('/') -> '/'
 * PathUtil.dirname('/abc') -> '/'
 * PathUtil.dirname('/abc/def') -> '/abc'
 * PathUtil.dirname('') -> ''
 *
 * @param {string} path The path to be parsed.
 * @return {string} The directory path.
 */
PathUtil.dirname = function(path) {
  var index = path.lastIndexOf('/');
  if (index < 0)
    return '';
  if (index == 0)
    return '/';
  return path.substring(0, index);
};

/**
 * Returns the base name (the last component) of the given |path|. If the
 * |path| ends with '/', returns an empty component.
 *
 * Examples:
 * PathUtil.basename('abc') -> 'abc'
 * PathUtil.basename('a/b') -> 'b'
 * PathUtil.basename('a/b/') -> ''
 * PathUtil.basename('a/b/c') -> 'c'
 * PathUtil.basename('/') -> ''
 * PathUtil.basename('/abc') -> 'abc'
 * PathUtil.basename('/abc/def') -> 'def'
 * PathUtil.basename('') -> ''
 *
 * @param {string} path The path to be parsed.
 * @return {string} The base name.
 */
PathUtil.basename = function(path) {
  var index = path.lastIndexOf('/');
  return index >= 0 ? path.substring(index + 1) : path;
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
 * @return {RootType} RootType.DOWNLOADS, RootType.DRIVE etc.
 */
PathUtil.getRootType = function(path) {
  var rootDir = PathUtil.getTopDirectory(path);
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

  if (type == RootType.DOWNLOADS || type == RootType.DRIVE_OFFLINE ||
      type == RootType.DRIVE_SHARED_WITH_ME || type == RootType.DRIVE_RECENT)
    return PathUtil.getTopDirectory(path);

  if (type == RootType.DRIVE || type == RootType.ARCHIVE ||
      type == RootType.REMOVABLE) {
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
 * Returns the localized name for the root type. If not available, then returns
 * null.
 *
 * @param {RootType} rootType The root type.
 * @return {?string} The localized name, or null if not available.
 */
PathUtil.getRootTypeLabel = function(rootType) {
  var str = function(id) {
    return loadTimeData.getString(id);
  };

  switch (rootType) {
    case RootType.DOWNLOADS:
      return str('DOWNLOADS_DIRECTORY_LABEL');
    case RootType.DRIVE:
      return str('DRIVE_MY_DRIVE_LABEL');
    case RootType.DRIVE_OFFLINE:
      return str('DRIVE_OFFLINE_COLLECTION_LABEL');
    case RootType.DRIVE_SHARED_WITH_ME:
      return str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL');
    case RootType.DRIVE_RECENT:
      return str('DRIVE_RECENT_COLLECTION_LABEL');
  }

  // Translation not found.
  return null;
};

/**
 * Extracts the extension of the path.
 *
 * Examples:
 * PathUtil.splitExtension('abc.ext') -> ['abc', '.ext']
 * PathUtil.splitExtension('a/b/abc.ext') -> ['a/b/abc', '.ext']
 * PathUtil.splitExtension('a/b') -> ['a/b', '']
 * PathUtil.splitExtension('.cshrc') -> ['', '.cshrc']
 * PathUtil.splitExtension('a/b.backup/hoge') -> ['a/b.backup/hoge', '']
 *
 * @param {string} path Path to be extracted.
 * @return {Array.<string>} Filename and extension of the given path.
 */
PathUtil.splitExtension = function(path) {
  var dotPosition = path.lastIndexOf('.');
  if (dotPosition <= path.lastIndexOf('/'))
    dotPosition = -1;

  var filename = dotPosition != -1 ? path.substr(0, dotPosition) : path;
  var extension = dotPosition != -1 ? path.substr(dotPosition) : '';
  return [filename, extension];
};
