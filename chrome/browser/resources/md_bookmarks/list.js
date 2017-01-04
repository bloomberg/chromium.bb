// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-list',

  properties: {
    /** @type {BookmarkTreeNode} */
    selectedNode: Object,
  },

  listeners: {
    'toggle-menu': 'onToggleMenu_'
  },

  /**
   * @param {Event} e
   * @private
   */
  onToggleMenu_: function(e) {
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
    this.closeDropdownMenu_();
  },

  /** @private */
  onDeleteTap_: function() {
    this.closeDropdownMenu_();
  },

  /** @private */
  closeDropdownMenu_: function() {
    var menu = /** @type {!CrActionMenuElement} */ (
        this.$.dropdown);
    menu.close();
  }
});
