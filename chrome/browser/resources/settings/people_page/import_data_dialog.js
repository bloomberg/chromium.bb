// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-import-data-dialog' is a component for importing
 * bookmarks and other data from other sources.
 */
Polymer({
  is: 'settings-import-data-dialog',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @private {!Array<!settings.BrowserProfile>} */
    browserProfiles_: Array,

    /** @private {!settings.BrowserProfile} */
    selected_: Object,

    prefs: Object,
  },

  /** @private {?settings.ImportDataBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.ImportDataBrowserProxyImpl.getInstance();
    this.browserProxy_.initializeImportDialog().then(function(data) {
      this.browserProfiles_ = data;
      this.selected_ = this.browserProfiles_[0];
    }.bind(this));

    this.addWebUIListener('import-data-status-changed', function(e) {
      // TODO(dpapad): Handle events to show spinner or success message.
    });

    this.$.dialog.showModal();
  },

  /** @private */
  isImportFromFileSelected_: function() {
    // The last entry in |browserProfiles_| always refers to dummy profile for
    // importing from a bookmarks file.
    return this.selected_.index == this.browserProfiles_.length - 1;
  },

  /**
   * @return {string}
   * @private
   */
  getActionButtonText_: function() {
    return this.i18n(this.isImportFromFileSelected_() ?
        'importChooseFile' : 'importCommit');
  },

  /** @private */
  onChange_: function() {
    this.selected_ = this.browserProfiles_[this.$.browserSelect.selectedIndex];
  },

  /** @private */
  onActionButtonTap_: function() {
    if (this.isImportFromFileSelected_()) {
      this.browserProxy_.importFromBookmarksFile();
    } else {
      // TODO(dpapad): Implement this.
    }
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },
});
