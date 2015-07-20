// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var Toolbar = Polymer({
    is: 'downloads-toolbar',

    /** @param {!downloads.ActionService} actionService */
    setActionService: function(actionService) {
      /** @private {!downloads.ActionService} */
      this.actionService_ = actionService;
    },

    properties: {
      canClearAll: {
        type: Boolean,
        value: false,
      },

      showingSearch_: {
        type: Boolean,
        value: false,
      },
    },

    /** @return {boolean} Whether removal can be undone. */
    canUndo: function() {
      return this.$['search-term'] != document.activeElement;
    },

    /** @private */
    onClearAllClick_: function() {
      this.actionService_.clearAll();
    },

    /** @private */
    onOpenDownloadsFolderClick_: function() {
      this.actionService_.openDownloadsFolder();
    },

    /** @private */
    toggleShowingSearch_: function() {
      this.showingSearch_ = !this.showingSearch_;
    },
  });

  return {Toolbar: Toolbar};
});
