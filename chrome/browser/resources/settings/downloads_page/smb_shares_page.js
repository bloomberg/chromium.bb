// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-smb-shares-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @private */
    showAddSmbDialog_: Boolean,
  },

  /** @override */
  attached: function() {
    this.addWebUIListener('on-add-smb-share', this.onAddShare_.bind(this));
  },

  /** @private */
  onAddShareTap_: function() {
    this.showAddSmbDialog_ = true;
  },

  /** @private */
  onAddSmbDialogClosed_: function() {
    this.showAddSmbDialog_ = false;
  },

  /**
   * @param {string} error_status
   * @private
   */
  onAddShare_: function(error_status) {
    this.$.addShareDoneMessage.textContent = error_status == 'success' ?
        loadTimeData.getString('smbShareAddedSuccessfulMessage') :
        loadTimeData.getString('smbShareAddedErrorMessage');
    this.$.errorToast.show();
  },

});
