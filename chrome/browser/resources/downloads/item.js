// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  /** @constructor */
  function Item() {}

  /**
   * The states a download can be in. These correspond to states defined in
   * DownloadsDOMHandler::CreateDownloadItemValue
   * @enum {string}
   */
  Item.States = {
    IN_PROGRESS: 'IN_PROGRESS',
    CANCELLED: 'CANCELLED',
    COMPLETE: 'COMPLETE',
    PAUSED: 'PAUSED',
    DANGEROUS: 'DANGEROUS',
    INTERRUPTED: 'INTERRUPTED',
  };

  /**
   * Explains why a download is in DANGEROUS state.
   * @enum {string}
   */
  Item.DangerType = {
    NOT_DANGEROUS: 'NOT_DANGEROUS',
    DANGEROUS_FILE: 'DANGEROUS_FILE',
    DANGEROUS_URL: 'DANGEROUS_URL',
    DANGEROUS_CONTENT: 'DANGEROUS_CONTENT',
    UNCOMMON_CONTENT: 'UNCOMMON_CONTENT',
    DANGEROUS_HOST: 'DANGEROUS_HOST',
    POTENTIALLY_UNWANTED: 'POTENTIALLY_UNWANTED',
  };

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
