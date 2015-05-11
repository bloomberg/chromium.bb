// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('viewer-zoom-button', {
  ready: function() {
    this.super();
    this.state_ = { opened: true };
  },

  show: function(delay) {
    if (!this.state_.opened)
      this.toggle_(delay);
  },

  hide: function(delay) {
    if (this.state_.opened)
      this.toggle_(delay);
  },

  toggle_: function(delay) {
    delay = delay || 0;
    this.state_.opened = !this.state_.opened;
  },

  activeChanged: function() {
    if (this.active)
      this.active = false;
  }
});
