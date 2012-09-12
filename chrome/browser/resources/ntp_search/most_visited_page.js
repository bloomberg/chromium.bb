// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp', function() {
  'use strict';

  var Thumbnail = ntp.Thumbnail;
  var ThumbnailPage = ntp.ThumbnailPage;

  /**
   * Creates a new Most Visited object for tiling.
   * @constructor
   * @extends {Thumbnail}
   * @extends {HTMLAnchorElement}
   * @param {Object} config Tile page configuration object.
   */
  function MostVisited(config) {
    var el = cr.doc.createElement('a');
    el.__proto__ = MostVisited.prototype;
    el.initialize(config);

    return el;
  }

  MostVisited.prototype = {
    __proto__: Thumbnail.prototype,

    /**
     * Initializes a MostVisited Thumbnail.
     * @param {Object} config TilePage configuration object.
     */
    initialize: function(config) {
      Thumbnail.prototype.initialize.apply(this, arguments);

      this.addEventListener('click', this.handleClick_);
      this.addEventListener('keydown', this.handleKeyDown_);
      this.addEventListener('carddeselected', this.handleCardDeselected_);
      this.addEventListener('cardselected', this.handleCardSelected_);
    },

    /**
     * Clears the DOM hierarchy for this node, setting it back to the default
     * for a blank thumbnail.
     */
    reset: function() {
      Thumbnail.prototype.reset.apply(this, arguments);

      var closeButton = cr.doc.createElement('div');
      closeButton.className = 'close-button';
      closeButton.title = loadTimeData.getString('removethumbnailtooltip');
      this.appendChild(closeButton);
    },

    /**
     * Update the appearance of this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     */
    updateForData: function(data) {
      if (this.classList.contains('blacklisted') && data) {
        // Animate appearance of new tile.
        this.classList.add('new-tile-contents');
      }
      this.classList.remove('blacklisted');

      Thumbnail.prototype.updateForData.apply(this, arguments);
    },

    /**
     * Handles a click on the tile.
     * @param {Event} e The click event.
     * @private
     */
    handleClick_: function(e) {
      if (e.target.classList.contains('close-button')) {
        this.blacklist_();
        e.preventDefault();
      } else {
        // Records an app launch from the most visited page (Chrome will decide
        // whether the url is an app). TODO(estade): this only works for clicks;
        // other actions like "open in new tab" from the context menu won't be
        // recorded. Can this be fixed?
        chrome.send('recordAppLaunchByURL',
                    [encodeURIComponent(this.href),
                     ntp.APP_LAUNCH.NTP_MOST_VISITED]);
        // Records the index of this tile.
        chrome.send('metricsHandler:recordInHistogram',
                    ['NewTabPage.MostVisited', this.index, 8]);
        chrome.send('mostVisitedAction',
                    [ntp.NtpFollowAction.CLICKED_TILE]);
      }
    },

    /**
     * Allow blacklisting most visited site using the keyboard.
     * @private
     */
    handleKeyDown_: function(e) {
      if (!cr.isMac && e.keyCode == 46 || // Del
          cr.isMac && e.metaKey && e.keyCode == 8) { // Cmd + Backspace
        this.blacklist_();
      }
    },

    /**
     * Permanently removes a page from Most Visited.
     * @private
     */
    blacklist_: function() {
      this.showUndoNotification_();
      chrome.send('blacklistURLFromMostVisited', [this.data_.url]);
      this.reset();
      chrome.send('getMostVisited');
      this.classList.add('blacklisted');
    },

    /**
     * Shows the undo notification when blacklisting a most visited site.
     * @private
     */
    showUndoNotification_: function() {
      var data = this.data_;
      var self = this;
      var doUndo = function() {
        chrome.send('removeURLsFromMostVisitedBlacklist', [data.url]);
        self.updateForData(data);
      };

      var undo = {
        action: doUndo,
        text: loadTimeData.getString('undothumbnailremove'),
      };

      var undoAll = {
        action: function() {
          chrome.send('clearMostVisitedURLsBlacklist');
        },
        text: loadTimeData.getString('restoreThumbnailsShort'),
      };

      ntp.showNotification(
          loadTimeData.getString('thumbnailremovednotification'),
          [undo, undoAll]);
    },

    /**
     * Returns whether this element can be 'removed' from chrome.
     * @return {boolean} True, since most visited pages can always be
     *     blacklisted.
     */
    canBeRemoved: function() {
      return true;
    }
  };

  /**
   * Creates a new MostVisitedPage object.
   * @constructor
   * @extends {TilePage}
   */
  function MostVisitedPage() {
    var el = new ThumbnailPage();
    el.__proto__ = MostVisitedPage.prototype;
    el.initialize();

    return el;
  }

  MostVisitedPage.prototype = {
    __proto__: ThumbnailPage.prototype,

    ThumbnailClass: MostVisited,

    /**
     * Initializes a MostVisitedPage.
     */
    initialize: function() {
      ThumbnailPage.prototype.initialize.apply(this, arguments);

      this.classList.add('most-visited-page');
    },

    /**
     * Handles the 'card deselected' event (i.e. the user clicked to another
     * pane).
     * @private
     * @param {Event} e The CardChanged event.
     */
    handleCardDeselected_: function(e) {
      if (!document.documentElement.classList.contains('starting-up')) {
        chrome.send('mostVisitedAction',
                    [ntp.NtpFollowAction.CLICKED_OTHER_NTP_PANE]);
      }
    },

    /**
     * Handles the 'card selected' event (i.e. the user clicked to select the
     * this page's pane).
     * @private
     * @param {Event} e The CardChanged event.
     */
    handleCardSelected_: function(e) {
      if (!document.documentElement.classList.contains('starting-up'))
        chrome.send('mostVisitedSelected');
    },

    /**
     * Sets the data that will be used to create Thumbnails.
     * TODO(pedrosimonetti): Move data handling related code to TilePage. Make
     * sure the new logic works with Apps without requiring duplicating code.
     * @param {Array} data The array of data.
     * @type (Array}
     */
    set data(data) {
      var startTime = Date.now();
      var maxTileCount = this.config_.maxTileCount;

      // The first time data is set, create the tiles.
      if (!this.data_) {
        this.data_ = data.slice(0, maxTileCount);
        this.createTiles_(this.data_.length);
      } else {
        this.data_ = refreshData(this.data_, data, maxTileCount);
      }

      this.updateTiles_();
      logEvent('mostVisited.layout: ' + (Date.now() - startTime));
    },
  };

  /**
   * Executed once the NTP has loaded. Checks if the Most Visited pane is
   * shown or not. If it is shown, the 'mostVisitedSelected' message is sent
   * to the C++ code, to record the fact that the user has seen this pane.
   */
  MostVisitedPage.onLoaded = function() {
    if (ntp.getCardSlider() &&
        ntp.getCardSlider().currentCardValue &&
        ntp.getCardSlider().currentCardValue.classList
        .contains('most-visited-page')) {
      chrome.send('mostVisitedSelected');
    }
  };

  /**
   * We've gotten additional Most Visited data. Update our old data with the
   * new data. The ordering of the new data is not important, except when a
   * page is pinned. Thus we try to minimize re-ordering.
   * @param {Array} oldData The current Most Visited page list.
   * @param {Array} newData The new Most Visited page list.
   * @return {Array} The merged page list that should replace the current page
   *     list.
   */
  function refreshData(oldData, newData, maxTileCount) {
    oldData = oldData.slice(0, maxTileCount);
    newData = newData.slice(0, maxTileCount);

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
    for (var i = 0; i < maxTileCount; i++) {
      if (oldData[i])
        oldData[i].updated = false;
    }

    return oldData;
  }

  return {
    MostVisitedPage: MostVisitedPage,
    refreshData: refreshData,
  };
});

document.addEventListener('ntpLoaded', ntp.MostVisitedPage.onLoaded);
