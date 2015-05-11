/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

Polymer('gaia-paper-button', {
  onKeyDown: function(e) {
    if (!this.disabled && (e.keyCode == 13 || e.keyCode == 32))
      this.fire('tap');
  }
});

Polymer('gaia-icon-button', {
  ready: function() {
    this.classList.add('custom-appearance');
  },

  onMouseDown: function(e) {
    /* Prevents button focusing after mouse click. */
    e.preventDefault();
  }
});

Polymer('gaia-raised-on-focus-button', {
  onButtonFocus: function() {
    this.raised = true;
  },

  onButtonBlur: function() {
    this.raised = false;
  },
});
