// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'content-panel',

  properties: {
    /**
     * The index of the selected page that is currently shown.
     * @private
     */
    selected_: {
      type: Number,
      value: 0,
    }
  },

  /**
   * Called when a page transition event occurs.
   * @param {Event} event
   * @private
   */
  onSelectedPageChanged_: function(event) {
    var newPage = event.detail.value instanceof Element &&
                  event.detail.value.children[0];
    if (newPage && newPage.activate != null) {
      newPage.activate();
    }
  }
});
