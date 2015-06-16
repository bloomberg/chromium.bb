/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

Polymer('gaia-button', {
  ready: function() {
    this.typeChanged();
  },

  onKeyDown: function(e) {
    if (!this.disabled && (e.keyCode == 13 || e.keyCode == 32)) {
      this.fire('tap');
      this.fire('click');
      e.stopPropagation();
    }
  },

  onFocus: function() {
    if (this.type == 'link' || this.type == 'dialog')
      return;
    this.raised = true;
  },

  onBlur: function() {
    if (this.type == 'link' || this.type == 'dialog')
      return;
    this.raised = false;
  },

  typeChanged: function() {
    if (this.type == 'link')
      this.setAttribute('noink', '');
    else
      this.removeAttribute('noink');
  },
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
