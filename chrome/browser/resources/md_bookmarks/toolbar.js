// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-toolbar',

  properties: {
    searchTerm: {
      type: String,
      observer: 'onSearchTermChanged_',
    },
  },

  /** @return {CrToolbarSearchFieldElement} */
  get searchField() {
    return /** @type {CrToolbarElement} */ (this.$$('cr-toolbar'))
        .getSearchField();
  },

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
  },

  /**
   * @param {Event} e
   * @private
   */
  onSearchChanged_: function(e) {
    var searchTerm = /** @type {string} */ (e.detail);
    this.fire('search-term-changed', searchTerm);
  },

  /** @private */
  onSearchTermChanged_: function() {
    if (!this.searchTerm)
      this.searchField.setValue('');
  },
});
