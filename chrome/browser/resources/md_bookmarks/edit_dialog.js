// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-edit-dialog',

  properties: {
    /** @private {BookmarkNode} */
    editItem_: Object,

    /** @private */
    isFolder_: Boolean,

    /** @private */
    titleValue_: String,

    /** @private */
    urlValue_: String,
  },

  /** @param {BookmarkNode} editItem */
  showEditDialog: function(editItem) {
    this.editItem_ = editItem;
    this.isFolder_ = !editItem.url;

    this.titleValue_ = editItem.title;
    if (!this.isFolder_)
      this.urlValue_ = assert(editItem.url);

    this.$.dialog.showModal();
  },

  /**
   * @param {boolean} isFolder
   * @return {string}
   * @private
   */
  getDialogTitle_: function(isFolder) {
    return loadTimeData.getString(
        isFolder ? 'renameFolderTitle' : 'editBookmarkTitle');
  },

  /** @private */
  onSaveButtonTap_: function() {
    // TODO(tsergeant): Save changes when enter is pressed.
    // TODO(tsergeant): Verify values.
    var edit = {'title': this.titleValue_};
    if (!this.isFolder_)
      edit['url'] = this.urlValue_;

    chrome.bookmarks.update(this.editItem_.id, edit);
    this.$.dialog.close();
  },

  /** @private */
  onCancelButtonTap_: function() {
    this.$.dialog.cancel();
  },
});
