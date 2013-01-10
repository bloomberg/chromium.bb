// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp', function() {
  'use strict';

  var Tile = ntp.Tile;
  var TilePage = ntp.TilePage;

  /**
   * Creates a new Thumbnail object for tiling.
   * @param {Object=} opt_data The data representing the thumbnail.
   * @constructor
   * @extends {Tile}
   * @extends {HTMLAnchorElement}
   */
  function Thumbnail(opt_data) {
    var el = cr.doc.createElement('a');
    el.__proto__ = Thumbnail.prototype;
    el.initialize();

    if (opt_data)
      el.data = opt_data;

    return el;
  }

  Thumbnail.prototype = Tile.subclass({
    __proto__: HTMLAnchorElement.prototype,

    /**
     * Initializes a Thumbnail.
     */
    initialize: function() {
      Tile.prototype.initialize.apply(this, arguments);
      this.classList.add('thumbnail');
      this.addEventListener('mouseover', this.handleMouseOver_);
    },

    /**
     * Clears the DOM hierarchy for this node, setting it back to the default
     * for a blank thumbnail.
     */
    reset: function() {
      this.innerHTML =
          '<span class="thumbnail-wrapper">' +
            '<span class="thumbnail-image thumbnail-card"></span>' +
          '</span>' +
          '<span class="title"></span>';

      this.tabIndex = -1;
      this.data_ = null;
      this.title = '';
    },

    /**
     * Update the appearance of this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     */
    set data(data) {
      Object.getOwnPropertyDescriptor(Tile.prototype, 'data').set.apply(this,
          arguments);

      this.formatThumbnail_(data);
    },

    /**
     * Formats this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     * @private
     */
    formatThumbnail_: function(data) {
      var title = this.querySelector('.title');
      title.textContent = data.title;
      title.dir = data.direction;

      // Sets the tooltip.
      this.title = data.title;

      var dataUrl = data.url;
      // Allow an empty string href (e.g. for a multiple tab thumbnail).
      this.href = typeof data.href != 'undefined' ? data.href : dataUrl;

      var thumbnailImage = this.querySelector('.thumbnail-image');

      var banner = thumbnailImage.querySelector('.thumbnail-banner');
      if (banner)
        thumbnailImage.removeChild(banner);

      var favicon = this.querySelector('.thumbnail-favicon') ||
                    this.ownerDocument.createElement('div');
      favicon.className = 'thumbnail-favicon';
      favicon.style.backgroundImage = getFaviconImageSet(dataUrl);
      this.appendChild(favicon);

      var self = this;
      var image = new Image();

      // If the thumbnail image fails to load, show an alternative presentation.
      // TODO(jeremycho): Move to a separate function?
      image.onerror = function() {
        banner = thumbnailImage.querySelector('.thumbnail-banner') ||
            self.ownerDocument.createElement('div');
        banner.className = 'thumbnail-banner';

        // For now, just strip leading http://www and trailing backslash.
        // TODO(jeremycho): Consult with UX on URL truncation.
        banner.textContent = dataUrl.replace(/^(http:\/\/)?(www\.)?|\/$/gi, '');
        thumbnailImage.appendChild(banner);
      };

      var thumbnailUrl = ntp.getThumbnailUrl(dataUrl);
      thumbnailImage.style.backgroundImage = url(thumbnailUrl);
      image.src = thumbnailUrl;
    },

    /**
     * Returns true if this is a thumbnail or descendant thereof.  Used to
     * detect when the mouse has transitioned into this thumbnail from a
     * strictly non-thumbnail element.
     * @param {Object} element The element to test.
     * @return {boolean} True if this is a thumbnail or a descendant thereof.
     * @private
     */
    isInThumbnail_: function(element) {
      while (element) {
        if (element == this)
          return true;
        element = element.parentNode;
      }
      return false;
    },

    /**
     * Increment the hover count whenever the mouse enters a thumbnail.
     * @param {Event} e The mouse over event.
     * @private
     */
    handleMouseOver_: function(e) {
      if (this.isInThumbnail_(e.target) &&
          !this.isInThumbnail_(e.relatedTarget)) {
        ntp.incrementHoveredThumbnailCount();
      }
    },
  });

  /**
   * Creates a new ThumbnailPage object.
   * @constructor
   * @extends {TilePage}
   */
  function ThumbnailPage() {
    var el = new TilePage();
    el.__proto__ = ThumbnailPage.prototype;

    return el;
  }

  ThumbnailPage.prototype = {
    __proto__: TilePage.prototype,

    /**
     * Initializes a ThumbnailPage.
     */
    initialize: function() {
      TilePage.prototype.initialize.apply(this, arguments);

      this.classList.add('thumbnail-page');
    },

    /** @override */
    shouldAcceptDrag: function(e) {
      return false;
    },
  };

  return {
    Thumbnail: Thumbnail,
    ThumbnailPage: ThumbnailPage,
  };
});
