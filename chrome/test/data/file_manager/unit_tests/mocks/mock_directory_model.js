// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock class for DirectoryModel.
 * @constructor
 * @extends {cr.EventTarget}
 */
function MockDirectoryModel() {
  this.fileFilter_ = new MockFileFilter();
}

/**
 * MockDirectoryModel inherits cr.EventTarget.
 */
MockDirectoryModel.prototype = {__proto__: cr.EventTarget.prototype};

/**
 * Returns a file filter.
 * @return {FileFilter} A file filter.
 */
MockDirectoryModel.prototype.getFileFilter = function() {
  return this.fileFilter_;
};

/**
 * Mock class for FileFilter.
 * @constructor
 * @extends {cr.EventTarget}
 */
function MockFileFilter() {}

/**
 * MockFileFilter extends cr.EventTarget.
 */
MockFileFilter.prototype = {__proto__: cr.EventTarget.prototype};

/**
 * Current implementation always returns true.
 * @param {Entry} entry File entry.
 * @return {boolean} True if the file should be shown, false otherwise.
 */
MockFileFilter.prototype.filter = function(entry) {
  return true;
};
