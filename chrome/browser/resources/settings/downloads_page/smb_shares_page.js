// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-smb-shares-page',

  properties: {
    /** @private */
    showAddSmbDialog_: Boolean,
  },

  /** @private */
  onAddShareTap_: function() {
    this.showAddSmbDialog_ = true;
  },

  /** @private */
  onAddSmbDialogClosed_: function() {
    this.showAddSmbDialog_ = false;
  },

});
