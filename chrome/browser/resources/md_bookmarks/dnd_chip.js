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

    /** @private */
    isMultiItem_: Boolean,
  },

  /**
   * @param {number} x
   * @param {number} y
   * @param {!Array<BookmarkNode>} items
   * @param {!BookmarkNode} dragItem
   */
  showForItems: function(x, y, items, dragItem) {
    this.style.setProperty('--mouse-x', x + 'px');
    this.style.setProperty('--mouse-y', y + 'px');

    if (this.showing_)
      return;

    this.isFolder_ = !dragItem.url;
    this.isMultiItem_ = items.length > 1;
    if (dragItem.url)
      this.$.icon.style.backgroundImage = cr.icon.getFavicon(dragItem.url);

    this.$.title.textContent = dragItem.title;
    this.$.count.textContent = items.length;
    this.showing_ = true;
  },

  hide: function() {
    this.showing_ = false;
  },
});
