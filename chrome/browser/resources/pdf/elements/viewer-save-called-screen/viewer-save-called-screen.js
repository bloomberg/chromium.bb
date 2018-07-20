// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-save-called-screen',

  properties: {
    strings: Object,
  },

  show: function() {
    this.$.dialog.showModal();
  },

  /** @private */
  dismiss_: function() {
    this.$.dialog.close();
  },
});
