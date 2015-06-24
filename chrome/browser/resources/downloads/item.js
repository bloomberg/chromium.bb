// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  /** @constructor */
  function Item() {}

  Item.prototype = {
    /** @type {downloads.ItemView} */
    view: null,

    /**
     * @param {!downloads.Data} data Info about the download.
     */
    render: function(data) {
      this.view = this.view || new downloads.ItemView;
      this.view.update(data);
    },

    unrender: function() {
      if (this.view) {
        this.view.destroy();
        this.view = null;
      }
    },
  };

  return {Item: Item};
});
