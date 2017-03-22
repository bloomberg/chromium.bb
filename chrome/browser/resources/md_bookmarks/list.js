// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-list',

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    /** @type {BookmarkNode} */
    menuItem_: Object,

    /** @private {Array<string>} */
    displayedList_: {
      type: Array,
      value: function() {
        // Use an empty list during initialization so that the databinding to
        // hide #bookmarksCard takes effect.
        return [];
      },
    },

    /** @private */
    searchTerm_: String,
  },

  listeners: {
    'open-item-menu': 'onOpenItemMenu_',
  },

  attached: function() {
    this.watch('displayedList_', function(state) {
      return bookmarks.util.getDisplayedList(state);
    });
    this.watch('searchTerm_', function(state) {
      return state.search.term;
    });
    this.updateFromStore();
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

  /** @private */
  onEditTap_: function() {
    this.closeDropdownMenu_();
    /** @type {BookmarksEditDialogElement} */ (this.$.editDialog.get())
        .showEditDialog(this.menuItem_);
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
    if (this.menuItem_.url) {
      chrome.bookmarks.remove(this.menuItem_.id, function() {
        // TODO(jiaxi): Add toast later.
      }.bind(this));
    } else {
      chrome.bookmarks.removeTree(this.menuItem_.id, function() {
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
  getEditActionLabel_: function() {
    var label = this.menuItem_.url ? 'menuEdit' : 'menuRename';
    return loadTimeData.getString(label);
  },

  /** @private */
  emptyListMessage_: function() {
    var emptyListMessage = this.searchTerm_ ? 'noSearchResults' : 'emptyList';
    return loadTimeData.getString(emptyListMessage);
  },

  /** @private */
  isEmptyList_: function() {
    return this.displayedList_.length == 0;
  },
});
