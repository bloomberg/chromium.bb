// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-toolbar',

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    /** @private */
    searchTerm_: {
      type: String,
      observer: 'onSearchTermChanged_',
    },

    sidebarWidth: {
      type: String,
      observer: 'onSidebarWidthChanged_',
    },

    showSelectionOverlay: {
      type: Boolean,
      computed: 'shouldShowSelectionOverlay_(selectedItems_, globalCanEdit_)',
      readOnly: true,
      reflectToAttribute: true,
    },

    /** @private */
    narrow_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private {!Set<string>} */
    selectedItems_: Object,

    /** @private */
    globalCanEdit_: Boolean,

    /** @private */
    selectedFolder_: String,

    /** @private */
    canChangeList_: {
      type: Boolean,
      computed:
          'computeCanChangeList_(selectedFolder_, searchTerm_, globalCanEdit_)',
    }
  },

  attached: function() {
    this.watch('searchTerm_', function(state) {
      return state.search.term;
    });
    this.watch('selectedItems_', function(state) {
      return state.selection.items;
    });
    this.watch('globalCanEdit_', function(state) {
      return state.prefs.canEdit;
    });
    this.watch('selectedFolder_', function(state) {
      return state.selectedFolder;
    });
    this.updateFromStore();
  },

  /** @return {CrToolbarSearchFieldElement} */
  get searchField() {
    return /** @type {CrToolbarElement} */ (this.$$('cr-toolbar'))
        .getSearchField();
  },

  /**
   * @param {Event} e
   * @private
   */
  onMenuButtonOpenTap_: function(e) {
    var menu = /** @type {!CrActionMenuElement} */ (this.$.dropdown.get());
    menu.showAt(/** @type {!Element} */ (e.target));
  },

  /** @private */
  onSortTap_: function() {
    chrome.bookmarkManagerPrivate.sortChildren(
        assert(this.getState().selectedFolder));
    bookmarks.ToastManager.getInstance().show(
        loadTimeData.getString('toastFolderSorted'), true);
    this.closeDropdownMenu_();
  },

  /** @private */
  onAddBookmarkTap_: function() {
    var dialog =
        /** @type {BookmarksEditDialogElement} */ (this.$.addDialog.get());
    dialog.showAddDialog(false, assert(this.getState().selectedFolder));
    this.closeDropdownMenu_();
  },

  /** @private */
  onAddFolderTap_: function() {
    var dialog =
        /** @type {BookmarksEditDialogElement} */ (this.$.addDialog.get());
    dialog.showAddDialog(true, assert(this.getState().selectedFolder));
    this.closeDropdownMenu_();
  },

  /** @private */
  onImportTap_: function() {
    chrome.bookmarks.import();
    this.closeDropdownMenu_();
  },

  /** @private */
  onExportTap_: function() {
    chrome.bookmarks.export();
    this.closeDropdownMenu_();
  },

  /** @private */
  onDeleteSelectionTap_: function() {
    var selection = this.getState().selection.items;
    var commandManager = bookmarks.CommandManager.getInstance();
    assert(commandManager.canExecute(Command.DELETE, selection));
    commandManager.handle(Command.DELETE, selection);
  },

  /** @private */
  onClearSelectionTap_: function() {
    this.dispatch(bookmarks.actions.deselectItems());
  },

  /** @private */
  closeDropdownMenu_: function() {
    var menu = /** @type {!CrActionMenuElement} */ (this.$.dropdown.get());
    menu.close();
  },

  /**
   * @param {Event} e
   * @private
   */
  onSearchChanged_: function(e) {
    var searchTerm = /** @type {string} */ (e.detail);
    if (searchTerm != this.searchTerm_)
      this.dispatch(bookmarks.actions.setSearchTerm(searchTerm));
  },

  /** @private */
  onSidebarWidthChanged_: function() {
    this.style.setProperty('--sidebar-width', this.sidebarWidth);
  },

  /** @private */
  onSearchTermChanged_: function() {
    this.searchField.setValue(this.searchTerm_ || '');
  },

  /**
   * @return {boolean}
   * @private
   */
  computeCanChangeList_: function() {
    return !this.searchTerm_ &&
        bookmarks.util.canReorderChildren(
            this.getState(), this.selectedFolder_);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSelectionOverlay_: function() {
    return this.selectedItems_.size > 1 && this.globalCanEdit_;
  },

  canDeleteSelection_: function() {
    return bookmarks.CommandManager.getInstance().canExecute(
        Command.DELETE, this.selectedItems_);
  },

  /**
   * @return {string}
   * @private
   */
  getItemsSelectedString_: function() {
    return loadTimeData.getStringF('itemsSelected', this.selectedItems_.size);
  },
});
