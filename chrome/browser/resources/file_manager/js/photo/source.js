// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * The album.
 *
 * @param {PhotoSource} source The parent source.
 * @param {string} name The album name.
 * @constructor
 */
function Album(source, name) {
  this.source_ = source;
  this.name_ = name;
  this.entries_ = null;
}

Album.prototype = { __proto__: cr.EventTarget.prototype };

/**
 * @return {string} The album name.
 */
Album.prototype.getName = function() {
  return this.name_;
};

/**
 * @return {Array.<FileEntry>} The entries list.
 */
Album.prototype.getEntries = function() {
  return this.entries_;
};

/**
 * @param {Array.<FileEntry>} entries The entries list.
 */
Album.prototype.setEntries = function(entries) {
  if (this.entries_ != null)
    throw 'entries must be null';

  this.entries_ = entries;
  cr.dispatchSimpleEvent(this, 'entries-ready');
};



/**
 * Ths source of all images inside the entry.
 * @param {FileEntry} entry Root entry.
 * @param {boolean} recurse Whether to recurse the subdirs.
 * @constructor
 */
function EntryPhotoSource(entry, recurse) {
  this.root_ = entry;
  this.recurse_ = recurse;
  this.album_ = new Album(this, this.root_.name);
  cr.dispatchSimpleEvent(this, 'albums-ready');

  var onTraversed = function(results) {
    this.album_.setEntries(results.filter(FileType.isImageOrVideo));
  }.bind(this);
  util.traverseTree(this.root_, onTraversed, recurse ? null : 1);
}

EntryPhotoSource.prototype = { __proto__: cr.EventTarget.prototype };

/**
 * @return {boolean} Whether the source is plain.
 */
EntryPhotoSource.prototype.isPlain = function() {
  return true;
};

/**
 * @return {string} The source type.
 */
EntryPhotoSource.prototype.getType = function() {
  return 'view';
};

/**
 * @return {Album} The album.
 */
EntryPhotoSource.prototype.getAlbums = function() {
  return [this.album_];
};



/**
 * The import source of images inside the entry.
 * This one detects whether we have already imported image or not.
 * @param {FileEntry} entry Root entry.
 * @constructor
 */
function ImportPhotoSource(entry) {
  this.root_ = entry;
  this.albums_ = null;

  var onTraversed = function(results) {
    var entries = results.filter(FileType.isImageOrVideo);
    var map = {};

    for (var index = 0; index < entries.length; index++) {
      var path = entries[index].fullPath;
      var parent = path.substring(0, path.lastIndexOf('/'));
      if (!(parent in map))
        map[parent] = [];
      map[parent].push(entries[index]);
    }

    this.albums_ = [];
    for (var index = 0; index < results.length; index++) {
      var path = results[index].fullPath;
      if (path in map) {
        var album = new Album(this, results[index].name);
        album.dirEntry = results[index];
        this.albums_.push(album);
        album.setEntries(map[path]);
      }
    }

    cr.dispatchSimpleEvent(this, 'albums-ready');
  }.bind(this);

  util.traverseTree(this.root_, onTraversed, null);
}

ImportPhotoSource.prototype = { __proto__: cr.EventTarget.prototype };

/**
 * @return {boolean} Whether the source is plain.
 */
ImportPhotoSource.prototype.isPlain = function() {
  return this.albums_ ? this.albums_.length == 1 : false;
};

/**
 * @return {string} The source type.
 */
ImportPhotoSource.prototype.getType = function() {
  return 'import';
};

/**
 * @return {Album} The album.
 */
ImportPhotoSource.prototype.getAlbums = function() {
  return this.albums_;
};
