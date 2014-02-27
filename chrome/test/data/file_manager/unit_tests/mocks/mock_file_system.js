// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock class for DOMFileSystem.
 * @param {string} volumeId Volume ID for the file system.
 * @constructor
 */
function MockFileSystem(volumeId) {
  this.name = '';
  this.root = new MockFileEntry(volumeId, '/');
}
