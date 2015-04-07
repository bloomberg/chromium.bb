// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('drop-down-button', {
  created: function() {
    /** @type {boolean} */
    this.iconShown = false;
  },

  /**
   * Fires a drop-down-button-click event. This is called when the element is
   * clicked. Switches the value of |iconShown|.
   */
  onButtonClick: function() {
    this.iconShown = !this.iconShown;
    this.fire('drop-down-button-click');
  }
});
