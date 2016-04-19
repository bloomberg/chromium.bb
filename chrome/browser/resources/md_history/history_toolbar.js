// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {SearchFieldDelegate}
 * @param {!HistoryToolbarElement} toolbar This history-toolbar.
 */
function ToolbarSearchFieldDelegate(toolbar) {
  this.toolbar_ = toolbar;
}

ToolbarSearchFieldDelegate.prototype = {
  /** @override */
  onSearchTermSearch: function(searchTerm) {
    this.toolbar_.onSearch(searchTerm);
  }
};

Polymer({
  is: 'history-toolbar',
  properties: {
    // Number of history items currently selected.
    count: {
      type: Number,
      value: 0,
      observer: 'changeToolbarView_'
    },

    // True if 1 or more history items are selected. When this value changes
    // the background colour changes.
    itemsSelected_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },

    // The most recent term entered in the search field. Updated incrementally
    // as the user types.
    searchTerm: {
      type: String,
      value: ''
    },

    // True if the page has a sidebar that we need to account for when
    // positioning toolbar items.
    hasSidebar: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    }
  },

  /**
   * Changes the toolbar background color depending on whether any history items
   * are currently selected.
   * @private
   */
  changeToolbarView_: function() {
    this.itemsSelected_ = this.count > 0;
  },

  /**
   * When changing the search term externally, update the search field to
   * reflect the new search term.
   * @param {string} search
   */
  setSearchTerm: function(search) {
    if (this.searchTerm == search)
      return;
    this.searchTerm = search;
    var searchField = /** @type {SearchField} */(this.$['search-input']);
    searchField.showAndFocus().then(function(showing) {
      if (showing) searchField.setValue(search);
    });
  },

  /**
   * If the search term has changed reload for the new search.
   */
  onSearch: function(searchTerm) {
    if (searchTerm != this.searchTerm) {
      this.searchTerm = searchTerm;
      this.fire('search-changed', {search: searchTerm});
    }
  },

  attached: function() {
    this.searchFieldDelegate_ = new ToolbarSearchFieldDelegate(this);
    /** @type {SearchField} */(this.$['search-input'])
        .setDelegate(this.searchFieldDelegate_);
  },

  onClearSelectionTap_: function() {
    this.fire('unselect-all');
  },

  /**
   * Relocates the user to the clear browsing data section of the settings page.
   * @private
   */
  onClearBrowsingDataTap_: function() {
    window.location.href = 'chrome://settings/clearBrowserData';
  },

  onDeleteTap_: function() {
    this.fire('delete-selected');
  },

  /**
   * If the user is a supervised user the delete button is not shown.
   * @private
   */
  deletingAllowed_: function() {
    return loadTimeData.getBoolean('allowDeletingHistory');
  },

  numberOfItemsSelected_: function(count) {
    return count > 0 ? loadTimeData.getStringF('itemsSelected', count) : '';
  }
});
