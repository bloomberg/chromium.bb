// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-toolbar',

  /**
   * @param {Event} e
   * @private
   */
  onMenuButtonOpenTap_: function(e) {
    var menu = /** @type {!CrActionMenuElement} */ (this.$.dropdown);
    menu.showAt(/** @type {!Element} */ (e.target));
  },

  /** @private */
  onBulkEditTap_: function() {
    this.closeDropdownMenu_();
  },

  /** @private */
  onSortTap_: function() {
    this.closeDropdownMenu_();
  },

  /** @private */
  onAddBookmarkTap_: function() {
    this.closeDropdownMenu_();
  },

  /** @private */
  onAddImportTap_: function() {
    this.closeDropdownMenu_();
  },

  /** @private */
  onAddExportTap_: function() {
    this.closeDropdownMenu_();
  },

  /** @private */
  closeDropdownMenu_: function() {
    var menu = /** @type {!CrActionMenuElement} */ (this.$.dropdown);
    menu.close();
  }
});
