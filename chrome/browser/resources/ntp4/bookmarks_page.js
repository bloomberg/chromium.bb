// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  var TilePage = ntp4.TilePage;

  var bookmarksPageGridValues = {
    // The fewest tiles we will show in a row.
    minColCount: 3,
    // The most tiles we will show in a row.
    maxColCount: 6,

    // The smallest a tile can be.
    minTileWidth: 100,
    // The biggest a tile can be.
    maxTileWidth: 100,
  };
  TilePage.initGridValues(bookmarksPageGridValues);

  /**
   * Creates a new BookmarksPage object.
   * @constructor
   * @extends {TilePage}
   */
  function BookmarksPage() {
    var el = new TilePage(bookmarksPageGridValues);
    el.__proto__ = BookmarksPage.prototype;
    el.initialize();

    return el;
  }

  BookmarksPage.prototype = {
    __proto__: TilePage.prototype,

    initialize: function() {
      this.classList.add('bookmarks-page');

      // TODO(csilv): Remove this placeholder.
      var placeholder = this.ownerDocument.createElement('div');
      placeholder.textContent = 'Bookmarks coming soon...';
      placeholder.className = 'placeholder';
      this.insertBefore(placeholder, this.firstChild);
    },

    /** @inheritDoc */
    shouldAcceptDrag: function(dataTransfer) {
      return false;
    }
  };

  return {
    BookmarksPage: BookmarksPage
  };
});
