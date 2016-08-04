// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-add-printer-dialog' is the dialog for setting up
 * a new CUPS printer.
 */
Polymer({
  is: 'settings-cups-add-printer-dialog',

  /** Opens the Add printer dialog. */
  open: function() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },
});
