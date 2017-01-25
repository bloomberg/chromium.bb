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

    // Whether domain-grouped history is enabled.
    isGroupedMode: {
      type: Boolean,
      reflectToAttribute: true,
    },

    // The period to search over. Matches BrowsingHistoryHandler::Range.
    groupedRange: {
      type: Number,
      reflectToAttribute: true,
    },

    groupedOffset: Number,

    // Show an (i) button on the right of the toolbar to display a notice about
    // synced history.
    showSyncNotice: {
      type: Boolean,
      observer: 'showSyncNoticeChanged_',
    },

    // Sync notice is currently visible.
    syncNoticeVisible_: {
      type: Boolean,
      value: false
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

  /**
   * If the user is a supervised user the delete button is not shown.
   * @private
   */
  deletingAllowed_: function() {
    return loadTimeData.getBoolean('allowDeletingHistory');
  },

  /** @private */
  numberOfItemsSelected_: function(count) {
    return count > 0 ? loadTimeData.getStringF('itemsSelected', count) : '';
  },

  /** @private */
  getHistoryInterval_: function() {
    var info = this.queryInfo;
    if (this.groupedRange == HistoryRange.WEEK)
      return info.queryInterval;

    if (this.groupedRange == HistoryRange.MONTH)
      return info.queryStartMonth;
  },

  /**
   * @param {Event} e
   * @private
   */
  onTabSelected_: function(e) {
    this.fire(
        'change-query', {range: Number(e.detail.item.getAttribute('value'))});
  },

  /**
   * @param {number} newOffset
   * @private
   */
  changeOffset_: function(newOffset) {
    if (!this.querying)
      this.fire('change-query', {offset: newOffset});
  },

  /** @private */
  onTodayTap_: function() {
    this.changeOffset_(0);
  },

  /** @private */
  onPrevTap_: function() {
    this.changeOffset_(this.groupedOffset + 1);
  },

  /** @private */
  onNextTap_: function() {
    this.changeOffset_(this.groupedOffset - 1);
  },

  /**
   * @private
   * @return {boolean}
   */
  isToday_: function() {
    return this.groupedOffset == 0;
  },
});
