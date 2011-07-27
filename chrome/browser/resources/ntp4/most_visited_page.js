// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  var TilePage = ntp4.TilePage;

  /**
   */
  var tileID = 0;

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

    get index() {
      assert(this.tile);
      return this.tile.index;
    },

    /**
     * Clears the DOM hierarchy for this node, setting it back to the default
     * for a blank thumbnail.
     */
    reset: function() {
      this.className = 'most-visited filler real';
      this.innerHTML =
          '<span class="thumbnail-wrapper fills-parent">' +
            '<div class="close-button"></div>' +
            '<span class="thumbnail fills-parent">' +
              // thumbnail-shield provides a gradient fade effect.
              '<div class="thumbnail-shield fills-parent"></div>' +
            '</span>' +
            '<span class="favicon"></span>' +
          '</span>' +
          '<div class="color-stripe"></div>' +
          '<span class="title"></span>';

      this.tabIndex = -1;
      this.data_ = null;
      this.removeAttribute('id');
    },

    /**
     * Update the appearance of this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     */
    updateForData: function(data) {
      if (!data || data.filler) {
        if (this.data_)
          this.reset();
        return;
      }

      var id = tileID++;
      this.setAttribute('id', 'tile' + id);
      this.data_ = data;
      this.tabIndex = 0;
      this.classList.remove('filler');

      var faviconDiv = this.querySelector('.favicon');
      var faviconUrl = data.faviconUrl ||
          'chrome://favicon/size/32/' + data.url;
      faviconDiv.style.backgroundImage = url(faviconUrl);
      faviconDiv.dir = data.direction;
      if (data.faviconDominantColor)
        this.setStripeColor(data.faviconDominantColor);
      else
        chrome.send('getFaviconDominantColor', [faviconUrl, id]);

      var title = this.querySelector('.title');
      title.textContent = data.title;
      title.dir = data.direction;

      var thumbnailUrl = data.thumbnailUrl || 'chrome://thumb/' + data.url;
      this.querySelector('.thumbnail').style.backgroundImage =
          url(thumbnailUrl);

      this.href = data.url;
    },

    /**
     * Sets the color of the favicon dominant color bar.
     * @param {string} color The css-parsable value for the color.
     */
    setStripeColor: function(color) {
      this.querySelector('.color-stripe').style.backgroundColor = color;
    },

    /**
     * Handles a click on the tile.
     * @param {Event} e The click event.
     */
    handleClick_: function(e) {
      if (e.target.classList.contains('close-button')) {
        this.blacklist_();
        e.preventDefault();
      } else {
        chrome.send('recordInHistogram', ['NTP_MostVisited', this.index, 8]);
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
     * Permanently removes a page from Most Visited.
     */
    blacklist_: function() {
      this.showUndoNotification_();
      chrome.send('blacklistURLFromMostVisited', [this.data_.url]);
      this.reset();
      chrome.send('getMostVisited');
    },

    showUndoNotification_: function() {
      var data = this.data_;
      var self = this;
      var doUndo = function () {
        chrome.send('removeURLsFromMostVisitedBlacklist', [data.url]);
        self.updateForData(data);
      }

      var undo = {
        action: doUndo,
        text: templateData.undothumbnailremove,
      }

      var undoAll = {
        action: function() {
          chrome.send('clearMostVisitedURLsBlacklist', []);
        },
        text: templateData.restoreThumbnailsShort,
      }

      ntp4.showNotification(templateData.thumbnailremovednotification,
                            [undo, undoAll]);
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
      this.style.right = x + 'px';
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
   * is based on the thumbnail, which should have a 212:132 ratio.
   * @return {number} The height.
   */
  function heightForWidth(width) {
    // The 2s are for borders, the 23 is for the title.
    return (width - 2) * 132 / 212 + 2 + 23;
  }

  var THUMBNAIL_COUNT = 8;

  /**
   * Creates a new MostVisitedPage object.
   * @constructor
   * @extends {TilePage}
   */
  function MostVisitedPage() {
    var el = new TilePage(mostVisitedPageGridValues);
    el.__proto__ = MostVisitedPage.prototype;
    el.initialize();

    return el;
  }

  MostVisitedPage.prototype = {
    __proto__: TilePage.prototype,

    initialize: function() {
      this.classList.add('most-visited-page');
      this.data_ = null;
      this.mostVisitedTiles_ = this.getElementsByClassName('most-visited real');
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
      var startTime = Date.now();

      // The first time data is set, create the tiles.
      if (!this.data_) {
        this.createTiles_();
        this.data_ = data.slice(0, THUMBNAIL_COUNT);
      } else {
        this.data_ = refreshData(this.data_, data);
      }

      this.updateTiles_();
      logEvent('mostVisited.layout: ' + (Date.now() - startTime));
    },

    /** @inheritDoc */
    shouldAcceptDrag: function(e) {
      return false;
    },

    /** @inheritDoc */
    heightForWidth: heightForWidth,
  };

  /**
   * We've gotten additional Most Visited data. Update our old data with the
   * new data. The ordering of the new data is not important, except when a
   * page is pinned. Thus we try to minimize re-ordering.
   * @param {Object} oldData The current Most Visited page list.
   * @param {Object} newData The new Most Visited page list.
   * @return The merged page list that should replace the current page list.
   */
  function refreshData(oldData, newData) {
    oldData = oldData.slice(0, THUMBNAIL_COUNT);
    newData = newData.slice(0, THUMBNAIL_COUNT);

    // Copy over pinned sites directly.
    for (var j = 0; j < newData.length; j++) {
      if (newData[j].pinned) {
        oldData[j] = newData[j];
        // Mark the entry as 'updated' so we don't try to update again.
        oldData[j].updated = true;
        // Mark the newData page as 'used' so we don't try to re-use it.
        newData[j].used = true;
      }
    }

    // Look through old pages; if they exist in the newData list, keep them
    // where they are.
    for (var i = 0; i < oldData.length; i++) {
      if (!oldData[i] || oldData[i].updated)
        continue;

      for (var j = 0; j < newData.length; j++) {
        if (newData[j].used)
          continue;

        if (newData[j].url == oldData[i].url) {
          // The background image and other data may have changed.
          oldData[i] = newData[j];
          oldData[i].updated = true;
          newData[j].used = true;
          break;
        }
      }
    }

    // Look through old pages that haven't been updated yet; replace them.
    for (var i = 0; i < oldData.length; i++) {
      if (oldData[i] && oldData[i].updated)
        continue;

      for (var j = 0; j < newData.length; j++) {
        if (newData[j].used)
          continue;

        oldData[i] = newData[j];
        oldData[i].updated = true;
        newData[j].used = true;
        break;
      }

      if (oldData[i] && !oldData[i].updated)
        oldData[i] = null;
    }

    // Clear 'updated' flags so this function will work next time it's called.
    for (var i = 0; i < THUMBNAIL_COUNT; i++) {
      if (oldData[i])
        oldData[i].updated = false;
    }

    return oldData;
  };

  function setFaviconDominantColor(id, color) {
    var tile = $('tile' + id);
    if (tile)
      tile.setStripeColor(color);
  };

  return {
    MostVisitedPage: MostVisitedPage,
    refreshData: refreshData,
    setFaviconDominantColor: setFaviconDominantColor,
  };
});
