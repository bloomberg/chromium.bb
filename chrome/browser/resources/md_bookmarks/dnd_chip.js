// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-dnd-chip',

  properties: {
    /** @private */
    showing_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private */
    isFolder_: Boolean,
  },

  /**
   * @param {number} x
   * @param {number} y
   * @param {!Array<BookmarkNode>} items
   */
  showForItems: function(x, y, items) {
    this.style.setProperty('--mouse-x', x + 'px');
    this.style.setProperty('--mouse-y', y + 'px');

    if (this.showing_)
      return;

    var firstItem = items[0];
    this.isFolder_ = !firstItem.url;
    // TODO(calamity): handle multi-item UI.
    if (firstItem.url)
      this.$.icon.style.backgroundImage = cr.icon.getFavicon(firstItem.url);

    this.$.title.textContent = firstItem.title;
    this.showing_ = true;
  },

  hide: function() {
    this.showing_ = false;
  },
});
