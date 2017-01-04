// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-folder-node',

  properties: {
    /** @type {BookmarkTreeNode} */
    item: Object,

    isSelected: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /**
   * @private
   * @return {string}
   */
  getFolderIcon_: function() {
    return this.isSelected ? 'bookmarks:folder-open' : 'bookmarks:folder';
  },

  /**
   * @private
   * @return {string}
   */
  getArrowIcon_: function() {
    return this.item.isOpen ? 'bookmarks:arrow-drop-up' :
                              'bookmarks:arrow-drop-down';
  },

  /** @private */
  selectFolder_: function() {
    this.fire('selected-folder-changed', this.item.id);
  },

  /**
   * Occurs when the drop down arrow is tapped.
   * @private
   */
  toggleFolder_: function() {
    this.fire('folder-open-changed', {
      id: this.item.id,
      open: !this.item.isOpen,
    });
  },

  /**
   * @private
   * @return {boolean}
   */
  hasChildFolder_: function() {
    for (var i = 0; i < this.item.children.length; i++) {
      if (!this.item.children[i].url)
        return true;
    }
    return false;
  },
});
