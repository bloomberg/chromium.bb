// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-item',

  properties: {
    /** @type {BookmarkTreeNode} */
    item: {
      type: Object,
      observer: 'onItemChanged_',
    },

    isFolder_: Boolean,

    isSelectedItem: {
      type: Boolean,
      reflectToAttribute: true,
    },
  },

  observers: [
    'updateFavicon_(item.url)',
  ],

  listeners: {
    'click': 'onClick_',
    'dblclick': 'onDblClick_',
  },

  /**
   * @param {Event} e
   * @private
   */
  onMenuButtonOpenClick_: function(e) {
    e.stopPropagation();
    this.fire('open-item-menu', {
      target: e.target,
      item: this.item,
    });
  },

  /** @private */
  onItemChanged_: function() {
    this.isFolder_ = !(this.item.url);
  },

  /**
   * @param {Event} e
   * @private
   */
  onClick_: function(e) {
    this.fire('select-item', {
      item: this.item,
      range: e.shiftKey,
      add: e.ctrlKey,
    });
  },

  /**
   * @param {Event} e
   * @private
   */
  onDblClick_: function(e) {
    if (!this.item.url)
      this.fire('selected-folder-changed', this.item.id);
    else
      chrome.tabs.create({url: this.item.url});
  },

  /**
   * @param {string} url
   * @private
   */
  updateFavicon_: function(url) {
    this.$.icon.style.backgroundImage = cr.icon.getFavicon(url);
  },
});
