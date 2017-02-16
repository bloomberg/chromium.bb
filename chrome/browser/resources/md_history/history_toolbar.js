// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-toolbar',
  properties: {
    // Number of history items currently selected.
    // TODO(calamity): bind this to
    // listContainer.selectedItem.selectedPaths.length.
    count: {
      type: Number,
      value: 0,
      observer: 'changeToolbarView_',
    },

    // True if 1 or more history items are selected. When this value changes
    // the background colour changes.
    itemsSelected_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    // The most recent term entered in the search field. Updated incrementally
    // as the user types.
    searchTerm: {
      type: String,
      observer: 'searchTermChanged_',
    },

    // True if the backend is processing and a spinner should be shown in the
    // toolbar.
    spinnerActive: {
      type: Boolean,
      value: false,
    },

    hasDrawer: {
      type: Boolean,
      reflectToAttribute: true,
    },

    // Show an (i) button on the right of the toolbar to display a notice about
    // synced history.
    showSyncNotice: {
      type: Boolean,
      observer: 'showSyncNoticeChanged_',
    },

    // Sync notice is currently visible.
    syncNoticeVisible_: {
      type: Boolean,
      value: false,
    },

    hasMoreResults: Boolean,

    querying: Boolean,

    queryInfo: Object,

    // Whether to show the menu promo (a tooltip that points at the menu button
    // in narrow mode).
    showMenuPromo: Boolean,
  },

  /**
   * True if the document currently has listeners to dismiss the sync notice,
   * which are added when the notice is first opened.
   * @private{boolean}
   */
  hasDismissListeners_: false,

  /** @private{?function(!Event)} */
  boundOnDocumentClick_: null,

  /** @private{?function(!Event)} */
  boundOnDocumentKeydown_: null,

  /** @override */
  detached: function() {
    if (this.hasDismissListeners_) {
      document.removeEventListener('click', this.boundOnDocumentClick_);
      document.removeEventListener('keydown', this.boundOnDocumentKeydown_);
    }
  },

  /** @return {CrToolbarSearchFieldElement} */
  get searchField() {
    return /** @type {CrToolbarElement} */ (this.$['main-toolbar'])
        .getSearchField();
  },

  showSearchField: function() {
    this.searchField.showAndFocus();
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
   * @private
   */
  searchTermChanged_: function() {
    if (this.searchField.getValue() != this.searchTerm) {
      this.searchField.showAndFocus();
      this.searchField.setValue(this.searchTerm);
    }
  },

  /** @private */
  showSyncNoticeChanged_: function() {
    if (!this.showSyncNotice)
      this.syncNoticeVisible_ = false;
  },

  /**
   * @param {!CustomEvent} event
   * @private
   */
  onSearchChanged_: function(event) {
    this.fire('change-query', {search: event.detail});
  },

  /**
   * @param {!MouseEvent} e
   * @private
   */
  onInfoButtonTap_: function(e) {
    this.syncNoticeVisible_ = !this.syncNoticeVisible_;
    e.stopPropagation();

    if (this.hasDismissListeners_)
      return;

    this.boundOnDocumentClick_ = this.onDocumentClick_.bind(this);
    this.boundOnDocumentKeydown_ = this.onDocumentKeydown_.bind(this);
    document.addEventListener('click', this.boundOnDocumentClick_);
    document.addEventListener('keydown', this.boundOnDocumentKeydown_);

    this.hasDismissListeners_ = true;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onDocumentClick_: function(e) {
    if (e.path.indexOf(this.$['sync-notice']) == -1)
      this.syncNoticeVisible_ = false;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onDocumentKeydown_: function(e) {
    if (e.key == 'Escape')
      this.syncNoticeVisible_ = false;
  },

  /** @private */
  onClearSelectionTap_: function() {
    this.fire('unselect-all');
  },

  /** @private */
  onDeleteTap_: function() {
    this.fire('delete-selected');
  },

  /** @private */
  numberOfItemsSelected_: function(count) {
    return count > 0 ? loadTimeData.getStringF('itemsSelected', count) : '';
  },
});
