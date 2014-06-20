// Copyright 2013 The Chromium Authors. All rights reserved.
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
