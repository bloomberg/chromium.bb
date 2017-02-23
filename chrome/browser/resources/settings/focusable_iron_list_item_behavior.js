// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @polymerBehavior */
var FocusableIronListItemBehavior = {
  properties: {
    /** @private {boolean} */
    focusedByKey_: Boolean,
  },

  listeners: {
    'keyup': 'onKeyUp_',
    'keydown': 'onKeyDown_',
    'mousedown': 'onMouseDown_',
    'blur': 'onBlur_',
  },

  /**
   * Flag that `this` is focused by keyboard, so mouse click doesn't apply
   * the .no-outline class.
   * @private
   */
  onKeyUp_: function() {
    // If refocusing on child, row itself isn't focused by keyboard anymore.
    if (!this.$$('[focused]'))
      this.focusedByKey_ = true;
  },

  /**
   * Unflag when moving away via keyboard (e.g. tabbing onto its children).
   * @private
   */
  onKeyDown_: function() {
    this.focusedByKey_ = false;
  },

  /**
   * When clicking on a row, do not show focus outline if the element wasn't
   * already in focus.
   * @private
   */
  onMouseDown_: function() {
    if (!this.focusedByKey_)
      this.classList.add('no-outline');
  },

  /**
   * Reset when moving away from the row entirely.
   * @private
   */
  onBlur_: function() {
    this.classList.remove('no-outline');
    this.focusedByKey_ = false;
  },
};