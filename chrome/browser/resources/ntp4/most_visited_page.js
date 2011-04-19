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
      this.reset();

      this.addEventListener('click', this.handleClick_.bind(this));
      this.addEventListener('keydown', this.handleKeyDown_.bind(this));
    },

    /**
     * Clears the DOM hierarchy for this node, setting it back to the default
     * for a blank thumbnail.
     */
    reset: function() {
      this.className = 'most-visited filler';
      // TODO(estade): why do we need edit-mode-border?
      this.innerHTML =
          '<div class="edit-mode-border fills-parent">' +
            '<div class="edit-bar-wrapper">' +
              '<div class="edit-bar">' +
                '<div class="pin"></div>' +
                '<div class="spacer"></div>' +
                '<div class="remove"></div>' +
              '</div>' +
            '</div>' +
            '<span class="thumbnail-wrapper fills-parent">' +
              '<span class="thumbnail fills-parent">' +
                // thumbnail-shield provides a gradient fade effect.
                '<div class="thumbnail-shield fills-parent"></div>' +
              '</span>' +
              '<span class="title"></span>' +
            '</span>' +
          '</div>';

      this.tabIndex = -1;
    },

    /**
     * Update the appearance of this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     */
    updateForData: function(data) {
      if (data.filler) {
        this.reset();
        return;
      }

      this.data_ = data;
      this.tabIndex = 0;
      this.classList.remove('filler');

      var title = this.querySelector('.title');
      title.textContent = data.title;
      var faviconUrl = data.faviconUrl || 'chrome://favicon/' + data.url;
      title.style.backgroundImage = url(faviconUrl);
      title.dir = data.direction;

      var thumbnailUrl = data.thumbnailUrl || 'chrome://thumb/' + data.url;
      this.querySelector('.thumbnail').style.backgroundImage =
          url(thumbnailUrl);

      this.href = data.url;

      this.updatePinnedState_();
    },

    /**
     * Handles a click on the tile.
     * @param {Event} e The click event.
     */
    handleClick_: function(e) {
      var target = e.target;
      if (target.classList.contains('pin')) {
        this.togglePinned_();
        e.preventDefault();
      } else if (target.classList.contains('remove')) {
        this.blacklist_();
        e.preventDefault();
      } else {
        var index = Array.prototype.indexOf.call(this.parentNode.children,
                                                 this);
        if (index != -1)
          chrome.send('metrics', ['NTP_MostVisited' + index]);
      }
    },

    /**
     * Allow blacklisting most visited site using the keyboard.
     */
    handleKeyDown_: function(e) {
      if (!IS_MAC && e.keyCode == 46 || // Del
          IS_MAC && e.metaKey && e.keyCode == 8) { // Cmd + Backspace
        this.blacklist_();
      }
    },

    /**
     * Changes the visual state of the page and updates the model.
     */
    togglePinned_: function() {
      var data = this.data_;
      data.pinned = !data.pinned;
      if (data.pinned) {
        chrome.send('addPinnedURL', [
          data.url,
          data.title,
          data.faviconUrl || '',
          data.thumbnailUrl || '',
          // TODO(estade): should not need to convert index to string.
          String(data.index)
        ]);
      } else {
        chrome.send('removePinnedURL', [data.url]);
      }

      this.updatePinnedState_();
    },

    /**
     * Updates the DOM for the current pinned state.
     */
    updatePinnedState_: function() {
      if (this.data_.pinned) {
        this.classList.add('pinned');
        this.querySelector('.pin').title = templateData.unpinthumbnailtooltip;
      } else {
        this.classList.remove('pinned');
        this.querySelector('.pin').title = templateData.pinthumbnailtooltip;
      }
    },

    /**
     * Permanently removes a page from Most Visited.
     */
    blacklist_: function() {
      chrome.send('blacklistURLFromMostVisited', [url]);
      this.reset();
      // TODO(estade): request a replacement site.
    },

    /**
     * Set the size and position of the most visited tile.
     * @param {number} size The total size of |this|.
     * @param {number} x The x-position.
     * @param {number} y The y-position.
     *     animate.
     */
    setBounds: function(size, x, y) {
      this.style.width = size + 'px';
      this.style.height = heightForWidth(size) + 'px';
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
   * Calculates the height for a Most Visited tile for a given width. The size
   * is based on the thumbnail, which should have a 212:132 ratio (the rest of
   * the arithmetic accounts for padding).
   * @return {number} The height.
   */
  function heightForWidth(width) {
    return (width - 6) * 132 / 212 + 29;
  }

  var THUMBNAIL_COUNT = 8;

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

      this.data_ = null;
      this.mostVisitedTiles_ = this.getElementsByClassName('most-visited');
    },

    /**
     * Create blank (filler) tiles.
     * @private
     */
    createTiles_: function() {
      for (var i = 0; i < THUMBNAIL_COUNT; i++) {
        this.appendTile(new MostVisited());
      }
    },

    /**
     * Update the tiles after a change to |data_|.
     */
    updateTiles_: function() {
      for (var i = 0; i < THUMBNAIL_COUNT; i++) {
        var page = this.data_[i];
        page.index = i;
        var tile = this.mostVisitedTiles_[i];

        if (i >= this.data_.length)
          tile.reset();
        else
          tile.updateForData(page);
      }
    },

    /**
     * Array of most visited data objects.
     * @type {Array}
     */
    get data() {
      return this.data_;
    },
    set data(data) {
      // The first time data is set, create the tiles.
      if (!this.data_)
        this.createTiles_();

      // We append the class name with the "filler" so that we can style fillers
      // differently.
      this.data_ = data.slice(0, THUMBNAIL_COUNT);
      this.updateTiles_();
    },

    /** @inheritDoc */
    heightForWidth: heightForWidth,
  };

  return {
    MostVisitedPage: MostVisitedPage,
  };
});
