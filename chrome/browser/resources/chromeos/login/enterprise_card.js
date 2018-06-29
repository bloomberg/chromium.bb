// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'enterprise-card',

  properties: {
    /**
     * Control visibility of the footer container.
     */
    noFooter: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  focus: function() {
    if (this.autofocus) {
      this.autofocus.focus();
    }
  },

  /** @override */
  ready: function() {
    this.autofocus = this.querySelector('.autofocus');
  },
});
