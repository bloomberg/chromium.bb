// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-side-bar',

  properties: {
    selectedPage: {
      type: String,
      notify: true
    }
  },

  /**
   * Handles menu selection changes.
   * @param {Event} e
   * @private
   */
  onSelect_: function(e) {
    this.fire('unselect-all');
    this.selectedPage = e.detail.item.getAttribute('view-id');
  },
});
