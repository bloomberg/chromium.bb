// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-item',

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    itemId: {
      type: String,
      observer: 'onItemIdChanged_',
    },

    /** @private {BookmarkNode} */
    item_: {
      type: Object,
      observer: 'onItemChanged_',
    },

    /** @private */
    isSelectedItem_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private */
    isFolder_: Boolean,
  },

  observers: [
    'updateFavicon_(item_.url)',
  ],

  listeners: {
    'click': 'onClick_',
    'dblclick': 'onDblClick_',
  },

  attached: function() {
    this.watch('item_', function(store) {
      return store.nodes[this.itemId];
    }.bind(this));

    this.updateFromStore();
  },

  /**
   * @param {Event} e
   * @private
   */
  onMenuButtonOpenClick_: function(e) {
    e.stopPropagation();
    this.fire('open-item-menu', {
      target: e.target,
      item: this.item_,
    });
  },

  /** @private */
  onItemIdChanged_: function() {
    // TODO(tsergeant): Add a histogram to measure whether this assertion fails
    // for real users.
    assert(this.getState().nodes[this.itemId]);
    this.updateFromStore();
  },

  /** @private */
  onItemChanged_: function() {
    this.isFolder_ = !(this.item_.url);
  },

  /**
   * @param {Event} e
   * @private
   */
  onClick_: function(e) {
    this.fire('select-item', {
      item: this.item_,
      range: e.shiftKey,
      add: e.ctrlKey,
    });
  },

  /**
   * @param {Event} e
   * @private
   */
  onDblClick_: function(e) {
    if (!this.item_.url)
      this.dispatch(bookmarks.actions.selectFolder(this.item_.id));
    else
      chrome.tabs.create({url: this.item_.url});
  },

  /**
   * @param {string} url
   * @private
   */
  updateFavicon_: function(url) {
    this.$.icon.style.backgroundImage = cr.icon.getFavicon(url);
  },
});
