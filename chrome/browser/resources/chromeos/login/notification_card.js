// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('notification-card', (function() {
  return {
    buttonClicked: function() {
      this.fire('buttonclick');
    },

    linkClicked: function(e) {
      this.fire('linkclick');
      e.preventDefault();
    }
  };
})());
