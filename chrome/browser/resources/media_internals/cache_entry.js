// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {

  /**
   * This class represents a file cached by net.
   */
  function CacheEntry() {
    this.read_ = new media.DisjointRangeSet;
    this.written_ = new media.DisjointRangeSet;
    this.available_ = new media.DisjointRangeSet;

    // Set to true when we know the entry is sparse.
    this.sparse = false;
    this.key = null;
    this.size = null;
  };

  CacheEntry.prototype = {
    /**
     * Mark a range of bytes as read from the cache.
     * @param {int} start The first byte read.
     * @param {int} length The number of bytes read.
     */
    readBytes: function(start, length) {
      start = parseInt(start);
      length = parseInt(length);
      this.read_.add(start, start + length);
      this.available_.add(start, start + length);
      this.sparse = true;
    },

    /**
     * Mark a range of bytes as written to the cache.
     * @param {int} start The first byte written.
     * @param {int} length The number of bytes written.
     */
    writeBytes: function(start, length) {
      start = parseInt(start);
      length = parseInt(length);
      this.written_.add(start, start + length);
      this.available_.add(start, start + length);
      this.sparse = true;
    },

    /**
     * Merge this CacheEntry with another, merging recorded ranges and flags.
     * @param {CacheEntry} other The CacheEntry to merge into this one.
     */
    merge: function(other) {
      this.read_.merge(other.read_);
      this.written_.merge(other.written_);
      this.available_.merge(other.available_);
      this.sparse = this.sparse || other.sparse;
      this.key = this.key || other.key;
      this.size = this.size || other.size;
    },

    /**
     * Render this CacheEntry as a <li>.
     * @return {HTMLElement} A <li> representing this CacheEntry.
     */
    toListItem: function() {
      var result = document.createElement('li');
      result.id = this.key;
      result.className = 'cache-entry';
      result.innerHTML = this.key + '<br />Read: ';
      result.innerHTML += this.read_.map(function(start, end) {
        return start + '-' + end;
      }).join(', ');

      result.innerHTML += '. Written: ';
      result.innerHTML += this.written_.map(function(start, end) {
        return start + '-' + end;
      }).join(', ');

      // Include the total size if we know it.
      if (this.size) {
        result.innerHTML += '. Out of ';
        result.innerHTML += this.size;
      }
      return result;
    }
  };

  return {
    CacheEntry: CacheEntry
  };
});
