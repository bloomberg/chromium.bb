// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('viewer-bookmarks-pane', {

  /**
   * @type {Array}
   * An array of top-level bookmarks, each containing a:
   * - title
   * - page (optional)
   * - children (an array of bookmarks)
   */
  bookmarks: null,

  toggle: function() {
    this.$.pane.toggle();
  }
});
