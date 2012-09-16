// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp', function() {
  'use strict';

  var Thumbnail = ntp.Thumbnail;
  var ThumbnailPage = ntp.ThumbnailPage;

 /**
  * The maximum number of thumbnails to render in a stack.
  * @type {number}
  * @const
  */
 var MAX_STACK_SIZE = 10;

 /**
  * The offset (in pixels) between two consecutive thumbnails on a stack.  This
  * should be kept in sync with .thumbnail-card's -webkit-margin-start.
  * @type {number}
  * @const
  */
 var STACK_OFFSET = 3;

  /**
   * Creates a new Recently Closed object for tiling.
   * @constructor
   * @extends {Thumbnail}
   * @extends {HTMLAnchorElement}
   * @param {Object} config Tile page configuration object.
   */
  function RecentlyClosed(config) {
    var el = cr.doc.createElement('a');
    el.__proto__ = RecentlyClosed.prototype;
    el.initialize(config);

    return el;
  }

  RecentlyClosed.prototype = {
    __proto__: Thumbnail.prototype,

    /**
     * Initializes a RecentlyClosed Thumbnail.
     * @param {Object} config TilePage configuration object.
     */
    initialize: function(config) {
      Thumbnail.prototype.initialize.apply(this, arguments);
      this.addEventListener('click', this.handleClick_);
    },

    /**
     * Formats this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     * @private
     */
    formatThumbnail_: function(data) {
      var newData = {};

      // Delete any existing blank thumbnails and reset image width.
      var thumbnailWrapper = this.querySelector('.thumbnail-wrapper');
      var blanks = thumbnailWrapper.querySelectorAll(
          '.thumbnail-card:not(.thumbnail-image)');
      for (var i = 0, length = blanks.length; i < length; ++i)
        thumbnailWrapper.removeChild(blanks[i]);

      var thumbnailImage = thumbnailWrapper.querySelector('.thumbnail-image');
      thumbnailImage.style.width = '100%';

      if (data.type == 'window') {
        newData.title = formatTabsText(data.tabs.length);
        newData.direction = isRTL() ? 'rtl' : 'ltr';
        newData.href = '';

        // Show a stack of blank thumbnails with the first tab on top.
        // TODO(jeremycho): Show the last focused tab on top instead.
        newData.url = data.tabs[0].url;

        var blanksCount = Math.min(data.tabs.length, MAX_STACK_SIZE) - 1;
        for (var i = 0; i < blanksCount; ++i) {
          var blank = this.ownerDocument.createElement('span');
          blank.className = 'thumbnail-card';
          thumbnailWrapper.insertBefore(blank, thumbnailImage);
        }
        // Thumbnail width - offset induced by the blank thumbnails.
        // TODO(jeremycho): Get the width from the config, either by storing the
        // object here or moving this function to TilePage.
        thumbnailImage.style.width = 130 - blanksCount * STACK_OFFSET + 'px';

      } else {
        newData = data;
      }
      Thumbnail.prototype.formatThumbnail_.call(this, newData);
    },

    /**
     * Handles a click on the tile.
     * @param {Event} e The click event.
     * @private
     */
    handleClick_: function(e) {
      ntp.logTimeToClickAndHoverCount('RecentlyClosed');
      var data = this.data;
      chrome.send('recordAppLaunchByURL',
                  [encodeURIComponent(data.url),
                  ntp.APP_LAUNCH.NTP_RECENTLY_CLOSED]);
      chrome.send('reopenTab', [data.sessionId, this.index,
          e.button, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey]);
    },
  };

  /**
   * Creates a new RecentlyClosedPage object.
   * @constructor
   * @extends {ThumbnailPage}
   */
  function RecentlyClosedPage() {
    var el = new ThumbnailPage();
    el.__proto__ = RecentlyClosedPage.prototype;
    el.initialize();

    return el;
  }

  RecentlyClosedPage.prototype = {
    __proto__: ThumbnailPage.prototype,

    ThumbnailClass: RecentlyClosed,

    /**
     * Initializes a RecentlyClosed Thumbnail.
     * @param {Object} config TilePage configuration object.
     */
    initialize: function(config) {
      ThumbnailPage.prototype.initialize.apply(this, arguments);
      this.classList.add('recently-closed-page');
    },

    /**
     * Sets the data that will be used to create Thumbnails.
     * @param {Array} data The array of data.
     */
    setData: function(data) {
      var startTime = Date.now();
      ThumbnailPage.prototype.setData.apply(this, arguments);

      // Only show the dot if we have recently closed tabs to display.
      var dot = this.navigationDot;
      if (this.data_.length == 0) {
        dot.hidden = true;
        // If the last tab has been removed (by a user click) and we're still on
        // the Recently Closed page, fall back to the first page in the dot
        // list.
        var cardSlider = ntp.getCardSlider();
        if (cardSlider.currentCardValue == this)
          cardSlider.selectCard(0, true);
      } else {
        dot.hidden = false;
      }

      logEvent('recentlyClosed.layout: ' + (Date.now() - startTime));
    }
  };

  /**
   * Returns the text used for a recently closed window.
   * @param {number} numTabs Number of tabs in the window.
   * @return {string} The text to use.
   */
  function formatTabsText(numTabs) {
    if (numTabs == 1)
      return loadTimeData.getString('closedwindowsingle');
    return loadTimeData.getStringF('closedwindowmultiple', numTabs);
  }

  return {
    RecentlyClosedPage: RecentlyClosedPage,
  };
});
