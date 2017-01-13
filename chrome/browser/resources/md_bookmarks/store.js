// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var BookmarksStore = Polymer({
  is: 'bookmarks-store',

  properties: {
    /** @type {BookmarkTreeNode} */
    rootNode: {
      type: Object,
      notify: true,
    },

    /** @type {?string} */
    selectedId: {
      type: String,
      observer: 'updateSelectedDisplay_',
      notify: true,
    },

    searchTerm: {
      type: String,
      observer: 'updateSearchDisplay_',
      notify: true,
    },

    /**
     * This updates to either the result of a search or the contents of the
     * selected folder.
     * @type {Array<BookmarkTreeNode>}
     */
    displayedList: {
      type: Array,
      notify: true,
      readOnly: true,
    },

    idToNodeMap_: Object,
  },

  /** @private {Object} */
  documentListeners_: null,

  /** @override */
  attached: function() {
    this.documentListeners_ = {
      'selected-folder-changed': this.onSelectedFolderChanged_.bind(this),
      'folder-open-changed': this.onFolderOpenChanged_.bind(this),
      'search-term-changed': this.onSearchTermChanged_.bind(this),
    };
    for (var event in this.documentListeners_)
      document.addEventListener(event, this.documentListeners_[event]);
  },

  /** @override */
  detached: function() {
    for (var event in this.documentListeners_)
      document.removeEventListener(event, this.documentListeners_[event]);
  },

  /**
   * Initializes the store with data from the bookmarks API.
   * Called by app on attached.
   */
  initializeStore: function() {
    chrome.bookmarks.getTree(function(results) {
      this.setupStore_(results[0]);
    }.bind(this));
    // Attach bookmarks API listeners.
    chrome.bookmarks.onRemoved.addListener(this.onBookmarkRemoved_.bind(this));
    chrome.bookmarks.onChanged.addListener(this.onBookmarkChanged_.bind(this));
  },

////////////////////////////////////////////////////////////////////////////////
// bookmarks-store, private:

  /**
   * @param {BookmarkTreeNode} rootNode
   * @private
   */
  setupStore_: function(rootNode) {
    this.rootNode = rootNode;
    this.idToNodeMap_ = {};
    this.rootNode.path = 'rootNode';
    BookmarksStore.generatePaths(rootNode, 0);
    BookmarksStore.initNodes(this.rootNode, this.idToNodeMap_);
    this.fire('selected-folder-changed', this.rootNode.children[0].id);
  },

  /** @private */
  deselectFolders_: function() {
    this.unlinkPaths('displayedList');
    this.set(this.idToNodeMap_[this.selectedId].path + '.isSelected', false);
    this.selectedId = null;
  },

  /**
   * @param {BookmarkTreeNode} folder
   * @private
   * @return {boolean}
   */
  isAncestorOfSelected_: function(folder) {
    if (!this.selectedId)
      return false;

    var selectedNode = this.idToNodeMap_[this.selectedId];
    return selectedNode.path.startsWith(folder.path);
  },

  /** @private */
  updateSearchDisplay_: function() {
    if (this.searchTerm == '') {
      this.fire('selected-folder-changed', this.rootNode.children[0].id);
    } else {
      chrome.bookmarks.search(this.searchTerm, function(results) {
        if (this.selectedId)
          this.deselectFolders_();

        this._setDisplayedList(results);
      }.bind(this));
    }
  },

  /** @private */
  updateSelectedDisplay_: function() {
    // Don't change to the selected display if ID was cleared.
    if (!this.selectedId)
      return;

    var selectedNode = this.idToNodeMap_[this.selectedId];
    this.linkPaths('displayedList', selectedNode.path + '.children');
    this._setDisplayedList(selectedNode.children);
  },

  /**
   * Remove all descendants of a given node from the map.
   * @param {string} id
   * @private
   */
  removeDescendantsFromMap_: function(id) {
    var node = this.idToNodeMap_[id];
    if (!node)
      return;

    if (node.children) {
      for (var i = 0; i < node.children.length; i++)
        this.removeDescendantsFromMap_(node.children[i].id);
    }
    delete this.idToNodeMap_[id];
  },

////////////////////////////////////////////////////////////////////////////////
// bookmarks-store, bookmarks API event listeners:

  /**
   * Callback for when a bookmark node is removed.
   * If a folder is selected or is an ancestor of a selected folder, the parent
   * of the removed folder will be selected.
   * @param {string} id The id of the removed bookmark node.
   * @param {!{index: number,
   *           parentId: string,
   *           node: BookmarkTreeNode}} removeInfo
   */
  onBookmarkRemoved_: function(id, removeInfo) {
    if (this.isAncestorOfSelected_(this.idToNodeMap_[id]))
      this.fire('selected-folder-changed', removeInfo.parentId);

    var parentNode = this.idToNodeMap_[removeInfo.parentId];
    this.splice(parentNode.path + '.children', removeInfo.index, 1);
    this.removeDescendantsFromMap_(id);
    BookmarksStore.generatePaths(parentNode, removeInfo.index);

    // Regenerate the search list if its displayed.
    if (this.searchTerm)
      this.updateSearchDisplay_();
  },

  /**
   * Called when the title of a bookmark changes.
   * @param {string} id The id of changed bookmark node.
   * @param {!Object} changeInfo
   */
  onBookmarkChanged_: function(id, changeInfo) {
    if (changeInfo.title)
      this.set(this.idToNodeMap_[id].path + '.title', changeInfo.title);
    if (changeInfo.url)
      this.set(this.idToNodeMap_[id].path + '.url', changeInfo.url);

    if (this.searchTerm)
      this.updateSearchDisplay_();
  },

////////////////////////////////////////////////////////////////////////////////
// bookmarks-store, bookmarks app event listeners:

  /**
   * @param {Event} e
   * @private
   */
  onSearchTermChanged_: function(e) {
    this.searchTerm = /** @type {string} */ (e.detail);
  },

  /**
   * Selects the folder specified by the event and deselects the previously
   * selected folder.
   * @param {CustomEvent} e
   * @private
   */
  onSelectedFolderChanged_: function(e) {
    if (this.searchTerm)
      this.searchTerm = '';

    // Deselect the old folder if defined.
    if (this.selectedId)
      this.set(this.idToNodeMap_[this.selectedId].path + '.isSelected', false);

    var selectedId = /** @type {string} */ (e.detail);
    var newFolder = this.idToNodeMap_[selectedId];
    this.set(newFolder.path + '.isSelected', true);
    this.selectedId = selectedId;
  },

  /**
   * Handles events that open and close folders.
   * @param {CustomEvent} e
   * @private
   */
  onFolderOpenChanged_: function(e) {
    var folder = this.idToNodeMap_[e.detail.id];
    this.set(folder.path + '.isOpen', e.detail.open);
    if (!folder.isOpen && this.isAncestorOfSelected_(folder))
      this.fire('selected-folder-changed', folder.id);
  },
});

////////////////////////////////////////////////////////////////////////////////
// bookmarks-store, static methods:

/**
 * Stores the path from the store to a node inside the node.
 * @param {BookmarkTreeNode} bookmarkNode
 * @param {number} startIndex
 */
BookmarksStore.generatePaths = function(bookmarkNode, startIndex) {
  if (!bookmarkNode.children)
    return;

  for (var i = startIndex; i < bookmarkNode.children.length; i++) {
    bookmarkNode.children[i].path = bookmarkNode.path + '.children.#' + i;
    BookmarksStore.generatePaths(bookmarkNode.children[i], 0);
  }
};

/**
 * Initializes the nodes in the bookmarks tree as follows:
 * - Populates |idToNodeMap_| with a mapping of all node ids to their
 *   corresponding BookmarkTreeNode.
 * - Sets all the nodes to not selected and open by default.
 * @param {BookmarkTreeNode} bookmarkNode
 * @param {Object=} idToNodeMap
 */
BookmarksStore.initNodes = function(bookmarkNode, idToNodeMap) {
  if (idToNodeMap)
    idToNodeMap[bookmarkNode.id] = bookmarkNode;

  if (bookmarkNode.url)
    return;

  bookmarkNode.isSelected = false;
  bookmarkNode.isOpen = true;
  for (var i = 0; i < bookmarkNode.children.length; i++)
    BookmarksStore.initNodes(bookmarkNode.children[i], idToNodeMap);
};
