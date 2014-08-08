// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Base class of mock entries.
 *
 * @param {string} volumeId ID of the volume that contains the entry.
 * @param {string} fullpath Full path of the entry.
 * @constructor
 */
function MockEntry(volumeId, fullPath) {
  this.volumeId = volumeId;
  this.fullPath = fullPath;
}

/**
 * Returns fake URL.
 *
 * @return {string} Fake URL.
 */
MockEntry.prototype.toURL = function() {
  return 'filesystem:' + this.volumeId + this.fullPath;
};

/**
 * Mock class for FileEntry.
 *
 * @param {string} volumeId Id of the volume containing the entry.
 * @param {string} fullPath Full path for the entry.
 * @extends {MockEntry}
 * @constructor
 */
function MockFileEntry(volumeId, fullPath, metadata) {
  MockEntry.call(this, volumeId, fullPath);
  this.volumeId = volumeId;
  this.fullPath = fullPath;
  this.metadata_ = metadata;
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
 * Mock class for DirectoryEntry.
 *
 * @param {string} volumeId Id of the volume containing the entry.
 * @param {string} fullPath Full path for the entry.
 * @param {Object.<String, MockFileEntry|MockDirectoryEntry>} contents Map of
 *     path and MockEntry contained in the directory.
 * @extends {MockEntry}
 * @constructor
 */
function MockDirectoryEntry(volumeId, fullPath, contents) {
  MockEntry.call(this, volumeId, fullPath);
  this.contents_ = contents;
}

MockDirectoryEntry.prototype = {
  __proto__: MockEntry.prototype
};

/**
 * Returns a file under the directory.
 *
 * @param {string} path Path.
 * @param {Object} option Option.
 * @param {callback(MockFileEntry)} successCallback Success callback.
 * @param {callback(Object)} failureCallback Failure callback;
 */
MockDirectoryEntry.prototype.getFile = function(
    path, option, successCallback, failureCallback) {
  if (!this.contents_[path])
    failureCallback({name: util.FileError.NOT_FOUND_ERR});
  else if (!(this.contents_[path] instanceof MockFileEntry))
    failureCallback({name: util.FileError.TYPE_MISMATCH_ERR});
  else
    successCallback(this.contents_[path]);
};

/**
 * Returns a directory under the directory.
 *
 * @param {string} path Path.
 * @param {Object} option Option.
 * @param {callback(MockDirectoryEntry)} successCallback Success callback.
 * @param {callback(Object)} failureCallback Failure callback;
 */
MockDirectoryEntry.prototype.getDirectory =
    function(path, option, successCallback, failureCallback) {
  if (!this.contents_[path])
    failureCallback({name: util.FileError.NOT_FOUND_ERR});
  else if (!(this.contents_[path] instanceof MockDirectoryEntry))
    failureCallback({name: util.FileError.TYPE_MISMATCH_ERR});
  else
    successCallback(this.contents_[path]);
};
