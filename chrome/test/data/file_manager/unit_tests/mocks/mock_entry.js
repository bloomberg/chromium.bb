// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock class for FileEntry.
 *
 * @param {string} volumeId Id of the volume containing the entry.
 * @param {string} fullPath Full path for the entry.
 * @constructor
 */
function MockFileEntry(volumeId, fullPath) {
  this.volumeId = volumeId;
  this.fullPath = fullPath;
}

/**
 * Returns fake URL.
 *
 * @return {string} Fake URL.
 */
MockFileEntry.prototype.toURL = function() {
  return 'filesystem:' + this.volumeId + this.fullPath;
};

/**
 * Mock class for DirectoryEntry.
 *
 * @param {string} volumeId Id of the volume containing the entry.
 * @param {string} fullPath Full path for the entry.
 * @param {Object.<String, MockFileEntry|MockDirectoryEntry>} contents Map of
 *     path and MockEntry contained in the directory.
 * @constructor
 */
function MockDirectoryEntry(volumeId, fullPath, contents) {
  this.contents_ = contents;
}

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
