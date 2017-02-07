// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-list',

  properties: {
    /** @type {BookmarkTreeNode} */
    menuItem_: Object,

    /** @type {Array<BookmarkTreeNode>} */
    displayedList: Array,

    searchTerm: String,
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

  /** @private */
  onEditTap_: function() {
    this.closeDropdownMenu_();
    this.$.editBookmark.showModal();
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
  onSaveEditTap_: function() {
    var edit = {'title': this.menuItem_.title};
    if (this.menuItem_.url)
      edit['url'] = this.menuItem_.url;

    chrome.bookmarks.update(this.menuItem_.id, edit);
    this.$.editBookmark.close();
  },

  /** @private */
  onCancelEditTap_: function() {
    this.$.editBookmark.cancel();
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
  getEditorTitle_: function() {
    var title = this.menuItem_.url ? 'editBookmarkTitle' : 'renameFolderTitle';
    return loadTimeData.getString(title);
  },

  /** @private */
  emptyListMessage_: function() {
    var emptyListMessage = this.searchTerm ? 'noSearchResults' : 'emptyList';
    return loadTimeData.getString(emptyListMessage);
  },

  /** @private */
  isEmptyList_: function() {
    return this.displayedList.length == 0;
  },
});
