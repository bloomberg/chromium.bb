// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-error-screen',
  properties: {
    strings: Object,

    reloadFn: {
      type: Object,
      value: null,
    }
  },

  show: function() {
    this.$.dialog.open();
  },

  reload: function() {
    if (this.reloadFn)
      this.reloadFn();
  }
});
