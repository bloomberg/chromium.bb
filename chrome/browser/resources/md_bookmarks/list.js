// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-list',

  properties: {
    /** @type {BookmarkTreeNode} */
    selectedNode: Object,

    /** @type {BookmarkTreeNode} */
    menuItem_: Object,
  },

  listeners: {
    'open-item-menu': 'onOpenItemMenu_',
  },

  /**
   * @param {Event} e
   * @private
   */
  onOpenItemMenu_: function(e) {
    this.menuItem_ = e.detail.item;
    var menu = /** @type {!CrActionMenuElement} */ (
        this.$.dropdown);
    menu.showAt(/** @type {!Element} */ (e.detail.target));
  },

  // TODO(jiaxi): change these dummy click event handlers later.
  /** @private */
  onEditTap_: function() {
    this.closeDropdownMenu_();
  },

  /** @private */
  onCopyURLTap_: function() {
    var idList = [this.menuItem_.id];
    chrome.bookmarkManagerPrivate.copy(idList, function() {
      // TODO(jiaxi): Add toast later.
    });
    this.closeDropdownMenu_();
  },

  /** @private */
  onDeleteTap_: function() {
    if (this.menuItem_.children) {
      chrome.bookmarks.removeTree(this.menuItem_.id, function() {
        // TODO(jiaxi): Add toast later.
      }.bind(this));
    } else {
      chrome.bookmarks.remove(this.menuItem_.id, function() {
        // TODO(jiaxi): Add toast later.
      }.bind(this));
    }
    this.closeDropdownMenu_();
  },

  /** @private */
  closeDropdownMenu_: function() {
    var menu = /** @type {!CrActionMenuElement} */ (
        this.$.dropdown);
    menu.close();
  },

  /** @private */
  isListEmpty_: function() {
    return this.selectedNode.children.length == 0;
  }
});
