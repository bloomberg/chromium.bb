/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

Polymer('gaia-input-form', (function() {
  return {
    onButtonClicked: function() {
      this.fire('submit');
    },

    onKeyDown: function(e) {
      if (e.keyCode == 13 && !this.$.button.disabled)
        this.onButtonClicked();
    }
  };
})());
