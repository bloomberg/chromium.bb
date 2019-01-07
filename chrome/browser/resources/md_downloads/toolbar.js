// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  const Toolbar = Polymer({
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

    /** @private {?mdDownloads.mojom.PageHandlerInterface} */
    mojoHandler_: null,

    /** @override */
    ready: function() {
      this.mojoHandler_ = downloads.BrowserProxy.getInstance().handler;
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

    focusOnSearchInput: function() {
      this.$.toolbar.getSearchField().showAndFocus();
    },

    isSearchFocused: function() {
      return this.$.toolbar.getSearchField().isSearchFocused();
    },

    /** @private */
    downloadsShowingChanged_: function() {
      this.updateClearAll_();
    },

    /** @private */
    onClearAllTap_: function() {
      assert(this.canClearAll());
      this.mojoHandler_.clearAll();
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
      const searchService = downloads.SearchService.getInstance();
      if (searchService.search(/** @type {string} */ (event.detail))) {
        this.spinnerActive = searchService.isSearching();
      }
      this.updateClearAll_();
    },

    /** @private */
    onOpenDownloadsFolderTap_: function() {
      this.mojoHandler_.openDownloadsFolderRequiringGesture();
      this.$.moreActionsMenu.close();
    },

    /** @private */
    updateClearAll_: function() {
      this.$$('.clear-all').hidden = !this.canClearAll();
    },
  });

  return {Toolbar: Toolbar};
});
