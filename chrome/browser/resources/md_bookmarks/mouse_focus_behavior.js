// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('bookmarks', function() {
  /** @const */
  var HIDE_FOCUS_RING_ATTRIBUTE = 'hide-focus-ring';

  /**
   * Behavior which adds the 'hide-focus-ring' attribute to a target element
   * when the user interacts with it using the mouse, allowing the focus outline
   * to be hidden without affecting keyboard users.
   * @polymerBehavior
   */
  var MouseFocusBehavior = {
    attached: function() {
      this.boundOnMousedown_ = this.onMousedown_.bind(this);
      this.boundOnKeydown = this.onKeydown_.bind(this);

      this.addEventListener('mousedown', this.boundOnMousedown_);
      this.addEventListener('keydown', this.boundOnKeydown, true);

    },

    detached: function() {
      this.removeEventListener('mousedown', this.boundOnMousedown_);
      this.removeEventListener('keydown', this.boundOnKeydown, true);
    },

    /** @private */
    onMousedown_: function() {
      this.setAttribute(HIDE_FOCUS_RING_ATTRIBUTE, '');
    },

    /** @private */
    onKeydown_: function() {
      this.removeAttribute(HIDE_FOCUS_RING_ATTRIBUTE);
    },
  };

  return {
    MouseFocusBehavior: MouseFocusBehavior,
  };
});
