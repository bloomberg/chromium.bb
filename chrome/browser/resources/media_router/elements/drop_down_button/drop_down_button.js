// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element is used as a button to toggle a drop down, such as
// the cast-mode-picker.
Polymer('drop-down-button', {
  /**
   * Whether or not to use the icon indicating that the drop down is shown.
   * @private {boolean}
   * @default false
   */
  useShownIcon_: false,

  /**
   * Fires a drop-down-button-click event. This is called when |this| is
   * clicked. Switches the value of |useShownIcon_|.
   */
  onButtonClick: function() {
    this.useShownIcon_ = !this.useShownIcon_;
    this.fire('drop-down-button-click');
  },
});
