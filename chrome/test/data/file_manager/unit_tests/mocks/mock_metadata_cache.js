// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock class for MetadataCache.
 * @constructor
 */
function MockMetadataCache() {
  this.metadata_ = {};
}

/**
 * Passes metadata for callback. Metadata can be set by setForTest.
 * @param {Array.<Entry>} entries The list of entries.
 * @param {string} type The metadata type.
 * @param {function(Object)} callback The metadata is passed to callback.
 */
MockMetadataCache.prototype.getLatest = function(entries, type, callback) {
  var result = [];
  for (var i = 0; i < entries.length; i++) {
    result[i] = this.getOne_(entries[i], type);
  }
  callback(result);
};

/**
 * Fetches metadata for one entry.
 * @param {Entry} entry The entry.
 * @param {string} type The metadata type.
 * @return {Object} A metadata. When no metadata is found for the entry and
 *     type, null is returned.
 * @private
 */
MockMetadataCache.prototype.getOne_ = function(entry, type) {
  if (this.metadata_[entry] && this.metadata_[entry][type])
    return this.metadata_[entry][type];
  else
    return null;
};

/**
 * Sets metadata for testing.
 * @param {Entry} entry The entry.
 * @param {string} type The metadata type.
 * @param {Object} value The metadata for the entry and the type.
 */
MockMetadataCache.prototype.setForTest = function(entry, type, value) {
  this.metadata_[entry][type] = value;
};
