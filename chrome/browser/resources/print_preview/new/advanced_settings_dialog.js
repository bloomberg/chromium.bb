// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-advanced-dialog',

  behaviors: [SettingsBehavior, I18nBehavior],

  properties: {
    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @private {?RegExp} */
    searchQuery_: {
      type: Object,
      value: null,
    },
  },

  /** @private */
  onCloseOrCancel_: function() {
    if (this.searchQuery)
      this.$.searchBox.setValue('');
  },

  /** @private */
  onCancelButtonClick_: function() {
    this.$.dialog.cancel();
  },

  show: function() {
    this.$.dialog.showModal();
  },

  close: function() {
    this.$.dialog.close();
  },
});
