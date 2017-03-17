// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-app',

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    /** @private */
    searchTerm_: {
      type: String,
      observer: 'searchTermChanged_',
    },

    /** @private */
    sidebarWidth_: String,
  },

  /** @private{?function(!Event)} */
  boundUpdateSidebarWidth_: null,

  /** @override */
  attached: function() {
    this.watch('searchTerm_', function(store) {
      return store.search.term;
    });

    chrome.bookmarks.getTree(function(results) {
      var nodeList = bookmarks.util.normalizeNodes(results[0]);
      var initialState = bookmarks.util.createEmptyState();
      initialState.nodes = nodeList;
      initialState.selectedFolder = nodeList['0'].children[0];

      bookmarks.Store.getInstance().init(initialState);
      bookmarks.ApiListener.init();

    }.bind(this));

    this.boundUpdateSidebarWidth_ = this.updateSidebarWidth_.bind(this);

    this.initializeSplitter_();
  },

  detached: function() {
    window.removeEventListener('resize', this.boundUpdateSidebarWidth_);
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
    if ('treeWidth' in window.localStorage) {
      splitterTarget.style.width = window.localStorage['treeWidth'];
      this.sidebarWidth_ = splitterTarget.getComputedStyleValue('width');
    }

    splitter.addEventListener('resize', function(e) {
      window.localStorage['treeWidth'] = splitterTarget.style.width;
      // TODO(calamity): This only fires when the resize is complete. This
      // should be updated on every width change.
      this.updateSidebarWidth_();
    }.bind(this));

    window.addEventListener('resize', this.boundUpdateSidebarWidth_);
  },

  /** @private */
  updateSidebarWidth_: function() {
    this.sidebarWidth_ = this.$.sidebar.getComputedStyleValue('width');
  },

  searchTermChanged_: function() {
    if (!this.searchTerm_)
      return;

    chrome.bookmarks.search(this.searchTerm_, function(results) {
      var ids = results.map(function(node) {
        return node.id;
      });
      this.dispatch(bookmarks.actions.setSearchResults(ids));
    }.bind(this));
  },
});
