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
      downloadsShowing: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
        observer: 'onDownloadsShowingChange_',
      },

      showingSearch_: {
        type: Boolean,
        value: false,
      },
    },

    /** @return {boolean} Whether removal can be undone. */
    canUndo: function() {
      return !this.$['search-term'].shadowRoot.activeElement;
    },

    /** @return {boolean} Whether "Clear all" should be allowed. */
    canClearAll: function() {
      return !this.$['search-input'].value && this.downloadsShowing;
    },

    /** @private */
    onClearAllClick_: function() {
      assert(this.canClearAll());
      this.actionService_.clearAll();
    },

    /** @private */
    onDownloadsShowingChange_: function() {
      this.updateClearAll_();
    },

    /** @private */
    onSearchTermSearch_: function() {
      this.actionService_.search(this.$['search-input'].value);
      this.updateClearAll_();
    },

    /** @private */
    onSearchTermKeydown_: function(e) {
      assert(this.showingSearch_);
      if (e.keyIdentifier == 'U+001B')  // Escape.
        this.toggleShowingSearch_();
    },

    /** @private */
    onOpenDownloadsFolderClick_: function() {
      this.actionService_.openDownloadsFolder();
    },

    /** @private */
    toggleShowingSearch_: function() {
      this.showingSearch_ = !this.showingSearch_;

      if (this.showingSearch_) {
        this.$['search-input'].focus();
      } else {
        this.$['search-input'].value = '';
        this.onSearchTermSearch_();
      }
    },

    /** @private */
    updateClearAll_: function() {
      this.$$('#actions .clear-all').hidden = !this.canClearAll();
      this.$$('paper-menu .clear-all').hidden = !this.canClearAll();
    },
  });

  return {Toolbar: Toolbar};
});

// TODO(dbeam): https://github.com/PolymerElements/iron-dropdown/pull/16/files
/** @suppress {checkTypes} */
(function() {
Polymer.IronDropdownScrollManager.pushScrollLock = function() {};
})();
