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
    'contextmenu': 'onContextMenu_',
  },

  /** @override */
  attached: function() {
    this.watch('item_', function(store) {
      return store.nodes[this.itemId];
    }.bind(this));
    this.watch('isSelectedItem_', function(store) {
      return !!store.selection.items.has(this.itemId);
    }.bind(this));

    this.updateFromStore();
  },

  /** @return {BookmarksItemElement} */
  getDropTarget: function() {
    return this;
  },

  /**
   * @param {Event} e
   * @private
   */
  onContextMenu_: function(e) {
    e.preventDefault();
    if (!this.isSelectedItem_) {
      this.dispatch(bookmarks.actions.selectItem(
          this.itemId, false, false, this.getState()));
    }
    this.fire('open-item-menu', {
      x: e.clientX,
      y: e.clientY,
    });
  },

  /**
   * @param {Event} e
   * @private
   */
  onMenuButtonClick_: function(e) {
    e.stopPropagation();
    this.dispatch(bookmarks.actions.selectItem(
        this.itemId, false, false, this.getState()));
    this.fire('open-item-menu', {
      targetElement: e.target,
    });
  },

  /**
   * @param {Event} e
   * @private
   */
  onMenuButtonDblClick_: function(e) {
    e.stopPropagation();
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
    this.isFolder_ = !this.item_.url;
  },

  /**
   * @param {Event} e
   * @private
   */
  onClick_: function(e) {
    this.dispatch(bookmarks.actions.selectItem(
        this.itemId, e.ctrlKey, e.shiftKey, this.getState()));
    e.stopPropagation();
  },

  /**
   * @param {Event} e
   * @private
   */
  onDblClick_: function(e) {
    if (!this.item_.url) {
      this.dispatch(
          bookmarks.actions.selectFolder(this.item_.id, this.getState().nodes));
    } else {
      chrome.tabs.create({url: this.item_.url});
    }
  },

  /**
   * @param {string} url
   * @private
   */
  updateFavicon_: function(url) {
    this.$.icon.style.backgroundImage = cr.icon.getFavicon(url);
  },
});
