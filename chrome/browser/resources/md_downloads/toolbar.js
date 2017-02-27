// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var Toolbar = Polymer({
    is: 'downloads-toolbar',

    properties: {
      downloadsShowing: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
        observer: 'downloadsShowingChanged_',
      },

      spinnerActive: {
        type: Boolean,
        notify: true,
      },
    },

    /** @return {boolean} Whether removal can be undone. */
    canUndo: function() {
      return !this.$.toolbar.getSearchField().isSearchFocused();
    },

    /** @return {boolean} Whether "Clear all" should be allowed. */
    canClearAll: function() {
      return this.getSearchText().length == 0 && this.downloadsShowing;
    },

    /** @return {string} The full text being searched. */
    getSearchText: function() {
      return this.$.toolbar.getSearchField().getValue();
    },

    onFindCommand: function() {
      this.$.toolbar.getSearchField().showAndFocus();
    },

    /** @private */
    downloadsShowingChanged_: function() {
      this.updateClearAll_();
    },

    /** @private */
    onClearAllTap_: function() {
      assert(this.canClearAll());
      downloads.ActionService.getInstance().clearAll();
      this.$.moreActionsMenu.close();
    },

    /** @private */
    onMoreActionsTap_: function() {
      this.$.moreActionsMenu.showAt(this.$.moreActions);
    },

    /**
     * @param {!CustomEvent} event
     * @private
     */
    onSearchChanged_: function(event) {
      var actionService = downloads.ActionService.getInstance();
      if (actionService.search(/** @type {string} */ (event.detail)))
        this.spinnerActive = actionService.isSearching();
      this.updateClearAll_();
    },

    /** @private */
    onOpenDownloadsFolderTap_: function() {
      downloads.ActionService.getInstance().openDownloadsFolder();
      this.$.moreActionsMenu.close();
    },

    /** @private */
    updateClearAll_: function() {
      this.$$('.clear-all').hidden = !this.canClearAll();
    },
  });

  return {Toolbar: Toolbar};
});
