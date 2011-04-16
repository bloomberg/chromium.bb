// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  var TilePage = ntp4.TilePage;

  /**
   * Creates a new Most Visited object for tiling.
   * @constructor
   * @extends {HTMLAnchorElement}
   */
  function MostVisited() {
    var el = cr.doc.createElement('a');
    el.__proto__ = MostVisited.prototype;
    el.initialize();

    return el;
  }

  MostVisited.prototype = {
    __proto__: HTMLAnchorElement.prototype,

    initialize: function() {
      this.className = 'most-visited filler';
      // TODO(estade): why do we need edit-mode-border?
      this.innerHTML =
          '<div class="edit-mode-border">' +
            '<div class="edit-bar">' +
              '<div class="pin"></div>' +
              '<div class="spacer"></div>' +
              '<div class="remove"></div>' +
            '</div>' +
            '<span class="thumbnail-wrapper">' +
              '<span class="thumbnail"></span>' +
            '</span>' +
          '</div>' +
          '<span class="title"></span>';
    },

    /**
     * Set the size and position of the most visited tile.
     * @param {number} size The total size of |this|.
     * @param {number} x The x-position.
     * @param {number} y The y-position.
     *     animate.
     */
    setBounds: function(size, x, y) {
      this.style.width = this.style.height = size + 'px';
      this.style.left = x + 'px';
      this.style.top = y + 'px';
    },
  };

  var mostVisitedPageGridValues = {
    // The fewest tiles we will show in a row.
    minColCount: 2,
    // The most tiles we will show in a row.
    maxColCount: 4,

    // TODO(estade): Change these to real values.
    // The smallest a tile can be.
    minTileWidth: 200,
    // The biggest a tile can be.
    maxTileWidth: 240,
  };
  TilePage.initGridValues(mostVisitedPageGridValues);

  /**
   * Creates a new MostVisitedPage object.
   * @param {string} name The display name for the page.
   * @constructor
   * @extends {TilePage}
   */
  function MostVisitedPage(name) {
    var el = new TilePage(name, mostVisitedPageGridValues);
    el.__proto__ = MostVisitedPage.prototype;
    el.initialize();

    return el;
  }

  MostVisitedPage.prototype = {
    __proto__: TilePage.prototype,

    initialize: function() {
      this.classList.add('most-visited-page');

      // We must let the CSS changes take effect before we add tiles so that
      // they will position correctly.
      window.setTimeout(this.createTiles_.bind(this), 0);
    },

    /**
     * Create blank (filler) tiles.
     * @private
     */
    createTiles_: function() {
      for (var i = 0; i < 8; i++) {
        this.appendTile(new MostVisited());
      }
    },
  };

  return {
    MostVisitedPage: MostVisitedPage,
  };
});
