// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var Toolbar = Polymer({
    is: 'downloads-toolbar',

    attached: function() {
      // isRTL() only works after i18n_template.js runs to set <html dir>.
      this.overflowAlign_ = isRTL() ? 'left' : 'right';

      /** @private {!SearchFieldDelegate} */
      this.searchFieldDelegate_ = new ToolbarSearchFieldDelegate(this);
      this.$['search-input'].setDelegate(this.searchFieldDelegate_);
    },

    properties: {
      downloadsShowing: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
        observer: 'onDownloadsShowingChange_',
      },

      overflowAlign_: {
        type: String,
        value: 'right',
      },
    },

    /** @return {boolean} Whether removal can be undone. */
    canUndo: function() {
      return this.$['search-input'] != this.shadowRoot.activeElement;
    },

    /** @return {boolean} Whether "Clear all" should be allowed. */
    canClearAll: function() {
      return !this.$['search-input'].getValue() && this.downloadsShowing;
    },

    /** @private */
    onClearAllTap_: function() {
      assert(this.canClearAll());
      downloads.ActionService.getInstance().clearAll();
    },

    /** @private */
    onDownloadsShowingChange_: function() {
      this.updateClearAll_();
    },

    /** @param {string} searchTerm */
    onSearchTermSearch: function(searchTerm) {
      downloads.ActionService.getInstance().search(searchTerm);
      this.updateClearAll_();
    },

    /** @private */
    onOpenDownloadsFolderTap_: function() {
      downloads.ActionService.getInstance().openDownloadsFolder();
    },

    /** @private */
    updateClearAll_: function() {
      this.$$('#actions .clear-all').hidden = !this.canClearAll();
      this.$$('paper-menu .clear-all').hidden = !this.canClearAll();
    },
  });

  /**
   * @constructor
   * @implements {SearchFieldDelegate}
   */
  // TODO(devlin): This is a bit excessive, and it would be better to just have
  // Toolbar implement SearchFieldDelegate. But for now, we don't know how to
  // make that happen with closure compiler.
  function ToolbarSearchFieldDelegate(toolbar) {
    this.toolbar_ = toolbar;
  }

  ToolbarSearchFieldDelegate.prototype = {
    /** @override */
    onSearchTermSearch: function(searchTerm) {
      this.toolbar_.onSearchTermSearch(searchTerm);
    }
  };

  return {Toolbar: Toolbar};
});

// TODO(dbeam): https://github.com/PolymerElements/iron-dropdown/pull/16/files
/** @suppress {checkTypes} */
(function() {
Polymer.IronDropdownScrollManager.pushScrollLock = function() {};
})();
