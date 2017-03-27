// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-edit-dialog',

  behaviors: [
    Polymer.IronA11yKeysBehavior,
  ],

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

  keyBindings: {
    'enter': 'onSaveButtonTap_',
  },

  /** @param {BookmarkNode} editItem */
  showEditDialog: function(editItem) {
    this.editItem_ = editItem;
    this.isFolder_ = !editItem.url;

    this.titleValue_ = editItem.title;
    if (!this.isFolder_) {
      this.$.url.invalid = false;
      this.urlValue_ = assert(editItem.url);
    }

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

  /**
   * Validates the value of the URL field, returning true if it is a valid URL.
   * May modify the value by prepending 'http://' in order to make it valid.
   * @return {boolean}
   * @private
   */
  validateUrl_: function() {
    var urlInput = /** @type {PaperInputElement} */ (this.$.url);
    var originalValue = this.urlValue_;

    if (urlInput.validate())
      return true;

    this.urlValue_ = 'http://' + originalValue;

    if (urlInput.validate())
      return true;

    this.urlValue_ = originalValue;
    return false;
  },

  /** @private */
  onSaveButtonTap_: function() {
    var edit = {'title': this.titleValue_};
    if (!this.isFolder_) {
      if (!this.validateUrl_())
        return;

      edit['url'] = this.urlValue_;
    }

    chrome.bookmarks.update(this.editItem_.id, edit);
    this.$.dialog.close();
  },

  /** @private */
  onCancelButtonTap_: function() {
    this.$.dialog.cancel();
  },
});
