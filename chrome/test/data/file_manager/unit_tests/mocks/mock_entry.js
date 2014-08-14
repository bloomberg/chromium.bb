// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Joins paths so that the two paths are connected by only 1 '/'.
 * @param {string} a Path.
 * @param {string} b Path.
 * @return {string} Joined path.
 */
function joinPath(a, b) {
  return a.replace(/\/+$/, '') + '/' + b.replace(/^\/+/, '');
};

/**
 * Test file system.
 *
 * @param {string} fileSystemId File system ID.
 * @constructor
 */
function TestFileSystem(fileSystemId) {
  this.fileSystemId = fileSystemId;
  this.entries = {};
};

TestFileSystem.prototype = {
  get root() { return this.entries['/']; }
};

/**
 * Base class of mock entries.
 *
 * @param {TestFileSystem} filesystem File system where the entry is localed.
 * @param {string} fullpath Full path of the entry.
 * @constructor
 */
function MockEntry(filesystem, fullPath) {
  this.filesystem = filesystem;
  this.fullPath = fullPath;
}

MockEntry.prototype = {
  /**
   * @return {string} Name of the entry.
   */
  get name() {
    return this.fullPath.replace(/^.*\//, '');
  }
};

/**
 * Returns fake URL.
 *
 * @return {string} Fake URL.
 */
MockEntry.prototype.toURL = function() {
  return 'filesystem:' + this.filesystem.fileSystemId + this.fullPath;
};

/**
 * Obtains parent directory.
 *
 * @param {function(MockDirectoryEntry)} onSuccess Callback invoked with
 *     the parent directory.
 * @param {function(Object)} onError Callback invoked with an error
 *     object.
 */
MockEntry.prototype.getParent = function(
    onSuccess, onError) {
  var path = this.fullPath.replace(/\/[^\/]+$/, '') || '/';
  if (this.filesystem.entries[path])
    onSuccess(this.filesystem.entries[path]);
  else
    onError({name: util.FileError.NOT_FOUND_ERR});
};

/**
 * Moves the entry to the directory.
 *
 * @param {MockDirectoryEntry} parent Destination directory.
 * @param {string=} opt_newName New name.
 * @param {function(MockDirectoryEntry)} onSuccess Callback invoked with the
 *     moved entry.
 * @param {function(Object)} onError Callback invoked with an error object.
 */
MockEntry.prototype.moveTo = function(parent, opt_newName, onSuccess, onError) {
  Promise.resolve().then(function() {
    this.filesystem.entries[this.fullPath] = null;
    return this.clone(joinPath(parent.fullPath, opt_newName || this.name));
  }.bind(this)).then(onSuccess, onError);
};

/**
 * Removes the entry.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function(Object)} onError Callback invoked with an error object.
 */
MockEntry.prototype.remove = function(onSuccess, onError) {
  Promise.resolve().then(function() {
    this.filesystem.entries[this.fullPath] = null;
  }.bind(this)).then(onSuccess, onError);
};

/**
 * Clones the entry with the new fullpath.
 *
 * @param {string} fullpath New fullpath.
 * @return {MockEntry} Cloned entry.
 */
MockEntry.prototype.clone = function(fullpath) {
  throw new Error('Not implemented.');
};

/**
 * Mock class for FileEntry.
 *
 * @param {FileSystem} filesystem File system where the entry is localed.
 * @param {string} fullPath Full path for the entry.
 * @param {Object} metadata Metadata.
 * @extends {MockEntry}
 * @constructor
 */
function MockFileEntry(filesystem, fullPath, metadata) {
  MockEntry.call(this, filesystem, fullPath);
  this.metadata_ = metadata;
  this.isFile = true;
  this.isDirectory = false;
}

MockFileEntry.prototype = {
  __proto__: MockEntry.prototype
};

/**
 * Obtains metadata of the entry.
 * @param {function(Object)} callback Function to take the metadata.
 */
MockFileEntry.prototype.getMetadata = function(callback) {
  Promise.resolve(this.metadata_).then(callback).catch(function(error) {
    console.error(error.stack || error);
    window.onerror();
  });
};

/**
 * @override
 */
MockFileEntry.prototype.clone = function(path) {
  return new MockFileEntry(this.filesystem, path, this.metadata);
};

/**
 * Mock class for DirectoryEntry.
 *
 * @param {FileSystem} filesystem File system where the entry is localed.
 * @param {string} fullPath Full path for the entry.
 * @extends {MockEntry}
 * @constructor
 */
function MockDirectoryEntry(filesystem, fullPath) {
  MockEntry.call(this, filesystem, fullPath);
  this.isFile = false;
  this.isDirectory = true;
}

MockDirectoryEntry.prototype = {
  __proto__: MockEntry.prototype
};

/**
 * @override
 */
MockDirectoryEntry.prototype.clone = function(path) {
  return new MockDirectoryEntry(this.filesystem, path);
};

/**
 * Returns a file under the directory.
 *
 * @param {string} path Path.
 * @param {Object} option Option.
 * @param {callback(MockFileEntry)} onSuccess Success callback.
 * @param {callback(Object)} onError Failure callback;
 */
MockDirectoryEntry.prototype.getFile = function(
    path, option, onSuccess, onError) {
  var fullPath = path[0] === '/' ? path : joinPath(this.fullPath, path);
  if (!this.filesystem.entries[fullPath])
    onError({name: util.FileError.NOT_FOUND_ERR});
  else if (!(this.filesystem.entries[fullPath] instanceof MockFileEntry))
    onError({name: util.FileError.TYPE_MISMATCH_ERR});
  else
    onSuccess(this.filesystem.entries[fullPath]);
};

/**
 * Returns a directory under the directory.
 *
 * @param {string} path Path.
 * @param {Object} option Option.
 * @param {callback(MockDirectoryEntry)} onSuccess Success callback.
 * @param {callback(Object)} onError Failure callback;
 */
MockDirectoryEntry.prototype.getDirectory =
    function(path, option, onSuccess, onError) {
  var fullPath = path[0] === '/' ? path : joinPath(this.fullPath, path);
  if (!this.filesystem.entries[fullPath])
    onError({name: util.FileError.NOT_FOUND_ERR});
  else if (!(this.filesystem.entries[fullPath] instanceof MockDirectoryEntry))
    onError({name: util.FileError.TYPE_MISMATCH_ERR});
  else
    onSuccess(this.filesystem.entries[fullPath]);
};
