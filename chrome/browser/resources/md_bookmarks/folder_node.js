// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-folder-node',

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    itemId: {
      type: String,
      observer: 'updateFromStore',
    },

    depth: {
      type: Number,
      observer: 'depthChanged_',
    },

    /** @type {BookmarkNode} */
    item_: Object,

    /** @private */
    isClosed_: Boolean,

    /** @private */
    selectedFolder_: String,

    /** @private */
    isSelectedFolder_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      computed: 'computeIsSelected_(itemId, selectedFolder_)'
    },
  },

  attached: function() {
    this.watch('item_', function(state) {
      return state.nodes[this.itemId];
    }.bind(this));
    this.watch('isClosed_', function(state) {
      return !!state.closedFolders[this.itemId];
    }.bind(this));
    this.watch('selectedFolder_', function(state) {
      return state.selectedFolder;
    });

    this.updateFromStore();
  },

  /**
   * @private
   * @return {string}
   */
  getFolderIcon_: function() {
    return this.isSelectedFolder_ ? 'bookmarks:folder-open' : 'cr:folder';
  },

  /** @private */
  selectFolder_: function() {
    this.dispatch(bookmarks.actions.selectFolder(this.item_.id));
  },

  /**
   * Occurs when the drop down arrow is tapped.
   * @private
   * @param {!Event} e
   */
  toggleFolder_: function(e) {
    this.dispatch(
        bookmarks.actions.changeFolderOpen(this.item_.id, this.isClosed_));
    e.stopPropagation();
  },

  /**
   * @private
   * @param {string} itemId
   * @param {string} selectedFolder
   * @return {boolean}
   */
  computeIsSelected_: function(itemId, selectedFolder) {
    return itemId == selectedFolder;
  },

  /**
   * @private
   * @return {boolean}
   */
  hasChildFolder_: function() {
    for (var i = 0; i < this.item_.children.length; i++) {
      if (this.isFolder_(this.item_.children[i]))
        return true;
    }
    return false;
  },

  /** @private */
  depthChanged_: function() {
    this.style.setProperty('--node-depth', String(this.depth));
  },

  /**
   * @private
   * @return {number}
   */
  getChildDepth_: function() {
    return this.depth + 1;
  },

  /**
   * @param {string} itemId
   * @private
   * @return {boolean}
   */
  isFolder_: function(itemId) {
    return !this.getState().nodes[itemId].url;
  }
});
