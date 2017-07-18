// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-app',

  behaviors: [
    bookmarks.MouseFocusBehavior,
    bookmarks.StoreClient,
  ],

  properties: {
    /** @private */
    searchTerm_: {
      type: String,
      observer: 'searchTermChanged_',
    },

    /** @type {ClosedFolderState} */
    closedFoldersState_: {
      type: Object,
      observer: 'closedFoldersStateChanged_',
    },

    /** @private */
    sidebarWidth_: String,
  },

  /** @private{?function(!Event)} */
  boundUpdateSidebarWidth_: null,

  /** @private {bookmarks.DNDManager} */
  dndManager_: null,

  /** @override */
  attached: function() {
    this.watch('searchTerm_', function(store) {
      return store.search.term;
    });

    this.watch('closedFoldersState_', function(store) {
      return store.closedFolders;
    });

    chrome.bookmarks.getTree(function(results) {
      var nodeMap = bookmarks.util.normalizeNodes(results[0]);
      var initialState = bookmarks.util.createEmptyState();
      initialState.nodes = nodeMap;
      initialState.selectedFolder = nodeMap[ROOT_NODE_ID].children[0];
      var closedFoldersString =
          window.localStorage[LOCAL_STORAGE_CLOSED_FOLDERS_KEY];
      initialState.closedFolders = closedFoldersString ?
          new Set(
              /** @type Array<string> */ (JSON.parse(closedFoldersString))) :
          new Set();

      bookmarks.Store.getInstance().init(initialState);
      bookmarks.ApiListener.init();

      setTimeout(function() {
        chrome.metricsPrivate.recordTime(
            'BookmarkManager.ResultsRenderedTime',
            Math.floor(window.performance.now()));
      });

    }.bind(this));

    this.boundUpdateSidebarWidth_ = this.updateSidebarWidth_.bind(this);

    this.initializeSplitter_();

    this.dndManager_ = new bookmarks.DNDManager();
    this.dndManager_.init();
  },

  detached: function() {
    window.removeEventListener('resize', this.boundUpdateSidebarWidth_);
    this.dndManager_.destroy();
  },

  /**
   * Set up the splitter and set the initial width from localStorage.
   * @private
   */
  initializeSplitter_: function() {
    var splitter = this.$.splitter;
    cr.ui.Splitter.decorate(splitter);
    var splitterTarget = this.$.sidebar;

    // The splitter persists the size of the left component in the local store.
    if (LOCAL_STORAGE_TREE_WIDTH_KEY in window.localStorage) {
      splitterTarget.style.width =
          window.localStorage[LOCAL_STORAGE_TREE_WIDTH_KEY];
    }
    this.sidebarWidth_ =
        /** @type {string} */ (getComputedStyle(splitterTarget).width);

    splitter.addEventListener('resize', function(e) {
      window.localStorage[LOCAL_STORAGE_TREE_WIDTH_KEY] =
          splitterTarget.style.width;
      this.updateSidebarWidth_();
    }.bind(this));

    splitter.addEventListener('dragmove', this.boundUpdateSidebarWidth_);
    window.addEventListener('resize', this.boundUpdateSidebarWidth_);
  },

  /** @private */
  updateSidebarWidth_: function() {
    this.sidebarWidth_ =
        /** @type {string} */ (getComputedStyle(this.$.sidebar).width);
  },

  /** @private */
  searchTermChanged_: function() {
    if (!this.searchTerm_)
      return;

    chrome.bookmarks.search(this.searchTerm_, function(results) {
      var ids = results.map(function(node) {
        return node.id;
      });
      this.dispatch(bookmarks.actions.setSearchResults(ids));
      this.fire('iron-announce', {
        text: ids.length > 0 ?
            loadTimeData.getStringF('searchResults', this.searchTerm_) :
            loadTimeData.getString('noSearchResults')
      });
    }.bind(this));
  },

  /** @private */
  closedFoldersStateChanged_: function() {
    window.localStorage[LOCAL_STORAGE_CLOSED_FOLDERS_KEY] =
        JSON.stringify(Array.from(this.closedFoldersState_));
  },
});
