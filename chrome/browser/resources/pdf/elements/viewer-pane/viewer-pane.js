// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('viewer-pane', {
    heading: '',

    ready: function() {
      // Prevent paper-dialog-base from scrolling back to top.
      this.$.dialog.openAction = function() {};
    },
    toggle: function() {
      this.$.dialog.toggle();
    }
});
