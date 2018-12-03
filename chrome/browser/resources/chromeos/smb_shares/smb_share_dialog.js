// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'smb-share-dialog' is used to host a <add-smb-share-dialog> element to
 * add SMB file shares.
 */

Polymer({
  is: 'smb-share-dialog',

  behaviors: [I18nBehavior],

  /** @private */
  onDialogClose_: function() {
    chrome.send('dialogClose');
  },
});