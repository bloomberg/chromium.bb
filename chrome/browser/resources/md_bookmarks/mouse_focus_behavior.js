// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('bookmarks', function() {
  /**
   * Behavior which adds the 'mouse-focus' class to a target element when it
   * gains focus from the mouse, allowing the outline to be hidden without
   * affecting keyboard users.
   * @polymerBehavior
   */
  var MouseFocusBehavior = {
    attached: function() {
      this.boundOnMousedown_ = this.onMousedown_.bind(this);
      this.boundClearMouseFocus_ = this.clearMouseFocus.bind(this);

      var target = this.getFocusTarget();
      target.addEventListener('mousedown', this.boundOnMousedown_);
      target.addEventListener('blur', this.boundClearMouseFocus_);
    },

    detached: function() {
      var target = this.getFocusTarget();
      target.removeEventListener('mousedown', this.boundOnMousedown_);
      target.removeEventListener('blur', this.boundClearMouseFocus_);
    },

    /**
     * Returns the element that should be watched for focus changes. Clients
     * can override, but it is assumed that the return value is constant over
     * the lifetime of the element.
     * @return {!HTMLElement}
     */
    getFocusTarget: function() {
      return this;
    },

    /** Reset the focus state when focus leaves the target element. */
    clearMouseFocus: function() {
      this.getFocusTarget().classList.remove('mouse-focus');
    },

    /** @private */
    onMousedown_: function() {
      this.getFocusTarget().classList.add('mouse-focus');
    },
  };

  return {
    MouseFocusBehavior: MouseFocusBehavior,
  };
});
