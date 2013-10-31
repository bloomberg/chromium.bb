// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Type of a root directory.
 * @enum
 */
var RootType = {
  DOWNLOADS: 'downloads',
  ARCHIVE: 'archive',
  REMOVABLE: 'removable',
  DRIVE: 'drive',
  DRIVE_OFFLINE: 'drive_offline',  // A fake root. Not the actual filesystem.
  DRIVE_SHARED_WITH_ME: 'drive_shared_with_me',  // A fake root.
  DRIVE_RECENT: 'drive_recent'  // A fake root.
};

/**
 * Top directory for each root type.
 * @type {Object.<RootType,string>}
 */
var RootDirectory = {
  DOWNLOADS: '/Downloads',
  ARCHIVE: '/archive',
  REMOVABLE: '/removable',
  DRIVE: '/drive',
  DRIVE_OFFLINE: '/drive_offline',  // A fake root. Not the actual filesystem.
  DRIVE_SHARED_WITH_ME: '/drive_shared_with_me',  // A fake root.
  DRIVE_RECENT: '/drive_recent'  // A fake root.
};

/**
 * Sub root directory for Drive. "root" and "other". This is not used now.
 * TODO(haruki): Add namespaces support. http://crbug.com/174233.
 * @enum
 */
var DriveSubRootDirectory = {
  ROOT: 'root',
  OTHER: 'other',
};

var PathUtil = {};

/**
 * The path to the default directory.
 * @type {string}
 * @const
 */
PathUtil.DEFAULT_DIRECTORY = RootDirectory.DOWNLOADS;

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
 * @param {string} path A root path.
 * @return {boolean} True if the given path is root and user can unmount it.
 */
PathUtil.isUnmountableByUser = function(path) {
  if (!PathUtil.isRootPath(path))
    return false;

  var type = PathUtil.getRootType(path);
  return (type == RootType.ARCHIVE || type == RootType.REMOVABLE);
};

/**
 * @param {string} parent_path The parent path.
 * @param {string} child_path The child path.
 * @return {boolean} True if |parent_path| is parent file path of |child_path|.
 */
PathUtil.isParentPath = function(parent_path, child_path) {
  if (!parent_path || parent_path.length == 0 ||
      !child_path || child_path.length == 0)
    return false;

  if (parent_path[parent_path.length - 1] != '/')
    parent_path += '/';

  if (child_path[child_path.length - 1] != '/')
    child_path += '/';

  return child_path.indexOf(parent_path) == 0;
};

/**
 * Return the localized name for the root.
 * @param {string} path The full path of the root (starting with slash).
 * @return {string} The localized name.
 */
PathUtil.getRootLabel = function(path) {
  var str = function(id) {
    return loadTimeData.getString(id);
  };

  if (path === RootDirectory.DOWNLOADS)
    return str('DOWNLOADS_DIRECTORY_LABEL');

  if (path === RootDirectory.ARCHIVE)
    return str('ARCHIVE_DIRECTORY_LABEL');
  if (PathUtil.isParentPath(RootDirectory.ARCHIVE, path))
    return path.substring(RootDirectory.ARCHIVE.length + 1);

  if (path === RootDirectory.REMOVABLE)
    return str('REMOVABLE_DIRECTORY_LABEL');
  if (PathUtil.isParentPath(RootDirectory.REMOVABLE, path))
    return path.substring(RootDirectory.REMOVABLE.length + 1);

  // TODO(haruki): Add support for "drive/root" and "drive/other".
  if (path === RootDirectory.DRIVE + '/' + DriveSubRootDirectory.ROOT)
    return str('DRIVE_DIRECTORY_LABEL');

  if (path === RootDirectory.DRIVE_OFFLINE)
    return str('DRIVE_OFFLINE_COLLECTION_LABEL');

  if (path === RootDirectory.DRIVE_SHARED_WITH_ME)
    return str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL');

  if (path === RootDirectory.DRIVE_RECENT)
    return str('DRIVE_RECENT_COLLECTION_LABEL');

  return path;
};

/**
 * Return the label of the folder to be shown. Eg.
 *  - '/foo/bar/baz' -> 'baz'
 *  - '/hoge/fuga/ -> 'fuga'
 * If the directory is root, returns the root label, which is same as
 * PathUtil.getRootLabel().
 *
 * @param {string} directoryPath The full path of the folder.
 * @return {string} The label to be shown.
 */
PathUtil.getFolderLabel = function(directoryPath) {
  var label = '';
  if (PathUtil.isRootPath(directoryPath))
    label = PathUtil.getRootLabel(directoryPath);

  if (label && label != directoryPath)
    return label;

  var matches = directoryPath.match(/([^\/]*)[\/]?$/);
  if (matches[1])
    return matches[1];

  return directoryPath;
};

/**
 * Returns if the given path can be a target path of folder shortcut.
 *
 * @param {string} directoryPath Directory path to be checked.
 * @return {boolean} True if the path can be a target path of the shortcut.
 */
PathUtil.isEligibleForFolderShortcut = function(directoryPath) {
  return !PathUtil.isSpecialSearchRoot(directoryPath) &&
         !PathUtil.isRootPath(directoryPath) &&
         PathUtil.isDriveBasedPath(directoryPath);
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
