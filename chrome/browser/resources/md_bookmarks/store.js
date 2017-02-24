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
      value: '',
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

    /** @type {Object<?string, !BookmarkTreeNode>} */
    idToNodeMap_: Object,

    /** @type {?number} */
    anchorIndex_: Number,

    /** @type {Set<string>} */
    searchResultSet_: Object,
  },

  /** @private {Object} */
  documentListeners_: null,

  /** @override */
  attached: function() {
    this.documentListeners_ = {
      'folder-open-changed': this.onFolderOpenChanged_.bind(this),
      'search-term-changed': this.onSearchTermChanged_.bind(this),
      'select-item': this.onItemSelected_.bind(this),
      'selected-folder-changed': this.onSelectedFolderChanged_.bind(this),
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
    chrome.bookmarks.onImportBegan.addListener(this.onImportBegan_.bind(this));
    chrome.bookmarks.onImportEnded.addListener(this.onImportEnded_.bind(this));
  },

  //////////////////////////////////////////////////////////////////////////////
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

    // Initialize the store's fields from the router.
    if (this.$.router.searchTerm)
      this.searchTerm = this.$.router.searchTerm;
    else
      this.fire('selected-folder-changed', this.$.router.selectedId);
  },

  /** @private */
  deselectFolders_: function() {
    this.unlinkPaths('displayedList');
    this.set(
        this.idToNodeMap_[this.selectedId].path + '.isSelectedFolder', false);
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
    if (!this.rootNode)
      return;

    if (!this.searchTerm) {
      this.fire('selected-folder-changed', this.rootNode.children[0].id);
    } else {
      chrome.bookmarks.search(this.searchTerm, function(results) {
        this.anchorIndex_ = null;
        this.clearSelectedItems_();
        this.searchResultSet_ = new Set();

        if (this.selectedId)
          this.deselectFolders_();

        this.setupSearchResults_(results);
      }.bind(this));
    }
  },

  /** @private */
  updateSelectedDisplay_: function() {
    // Don't change to the selected display if ID was cleared.
    if (!this.selectedId)
      return;

    this.clearSelectedItems_();
    this.anchorIndex_ = null;

    var selectedNode = this.idToNodeMap_[this.selectedId];
    this.linkPaths('displayedList', selectedNode.path + '.children');
    this._setDisplayedList(
        /** @type {Array<BookmarkTreeNode>} */ (selectedNode.children));
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

  /**
   * Remove all selected items in the list.
   * @private
   */
  clearSelectedItems_: function() {
    if (!this.displayedList)
      return;

    for (var i = 0; i < this.displayedList.length; i++) {
      if (!this.displayedList[i].isSelectedItem)
        continue;

      this.set('displayedList.#' + i + '.isSelectedItem', false);
    }
  },

  /**
   * Return the index in the search result of an item.
   * @param {BookmarkTreeNode} item
   * @return {number}
   * @private
   */
  getIndexInList_: function(item) {
    return this.searchTerm ? item.searchResultIndex : item.index;
  },

  /**
   * @param {string} id
   * @return {boolean}
   * @private
   */
  isInDisplayedList_: function(id) {
    return this.searchTerm ? this.searchResultSet_.has(id) :
                             this.idToNodeMap_[id].parentId == this.selectedId;
  },

  /**
   * Initializes the search results returned by the API as follows:
   * - Populates |searchResultSet_| with a mapping of all result ids to
   *   their corresponding result.
   * - Sets up the |searchResultIndex|.
   * @param {Array<BookmarkTreeNode>} results
   * @private
   */
  setupSearchResults_: function(results) {
    for (var i = 0; i < results.length; i++) {
      results[i].searchResultIndex = i;
      results[i].isSelectedItem = false;
      this.searchResultSet_.add(results[i].id);
    }

    this._setDisplayedList(results);
  },

  /**
   * Select multiple items based on |anchorIndex_| and the selected
   * item. If |anchorIndex_| is not set, single select the item.
   * @param {BookmarkTreeNode} item
   * @private
   */
  selectRange_: function(item) {
    var startIndex, endIndex;
    if (this.anchorIndex_ == null) {
      this.anchorIndex_ = this.getIndexInList_(item);
      startIndex = this.anchorIndex_;
      endIndex = this.anchorIndex_;
    } else {
      var selectedIndex = this.getIndexInList_(item);
      startIndex = Math.min(this.anchorIndex_, selectedIndex);
      endIndex = Math.max(this.anchorIndex_, selectedIndex);
    }
    for (var i = startIndex; i <= endIndex; i++)
      this.set('displayedList.#' + i + '.isSelectedItem', true);
  },

  /**
   * Selects a single item in the displayedList.
   * @param {BookmarkTreeNode} item
   * @private
   */
  selectItem_: function(item) {
    this.anchorIndex_ = this.getIndexInList_(item);
    this.set('displayedList.#' + this.anchorIndex_ + '.isSelectedItem', true);
  },

  //////////////////////////////////////////////////////////////////////////////
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
    chrome.bookmarks.getSubTree(removeInfo.parentId, function(parentNodes) {
      var parentNode = parentNodes[0];
      var isAncestor = this.isAncestorOfSelected_(this.idToNodeMap_[id]);
      var wasInDisplayedList = this.isInDisplayedList_(id);

      // Refresh the parent node's data from the backend as its children's
      // indexes will have changed and Polymer doesn't update them.
      this.removeDescendantsFromMap_(id);
      parentNode.path = this.idToNodeMap_[parentNode.id].path;
      BookmarksStore.generatePaths(parentNode, 0);
      BookmarksStore.initNodes(parentNode, this.idToNodeMap_);
      this.set(parentNode.path, parentNode);

      // Updates selectedId if the removed node is an ancestor of the current
      // selected node.
      if (isAncestor)
        this.fire('selected-folder-changed', removeInfo.parentId);

      // Only update the displayedList if the removed node is in the
      // displayedList.
      if (!wasInDisplayedList)
        return;

      this.anchorIndex_ = null;

      // Update the currently displayed list.
      if (this.searchTerm) {
        this.updateSearchDisplay_();
      } else {
        if (!isAncestor)
          this.fire('selected-folder-changed', this.selectedId);

        this._setDisplayedList(parentNode.children);
      }
    }.bind(this));
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

  /**
   * Called when importing bookmark is started.
   */
  onImportBegan_: function() {
    // TODO(rongjie): pause onCreated once this event is used.
  },

  /**
   * Called when importing bookmark node is finished.
   */
  onImportEnded_: function() {
    chrome.bookmarks.getTree(function(results) {
      this.setupStore_(results[0]);
      this.updateSelectedDisplay_();
    }.bind(this));
  },

  //////////////////////////////////////////////////////////////////////////////
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
    if (this.selectedId && this.idToNodeMap_[this.selectedId])
      this.set(
          this.idToNodeMap_[this.selectedId].path + '.isSelectedFolder', false);

    // Check if the selected id is that of a defined folder.
    var id = /** @type {string} */ (e.detail);
    if (!this.idToNodeMap_[id] || this.idToNodeMap_[id].url)
      id = this.rootNode.children[0].id;

    var folder = this.idToNodeMap_[id];
    this.set(folder.path + '.isSelectedFolder', true);
    this.selectedId = id;

    if (folder.id == this.rootNode.id)
      return;

    var parent = this.idToNodeMap_[/** @type {?string} */ (folder.parentId)];
    while (parent) {
      if (!parent.isOpen) {
        this.fire('folder-open-changed', {
          id: parent.id,
          open: true,
        });
      }

      parent = this.idToNodeMap_[/** @type {?string} */ (parent.parentId)];
    }
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

  /**
   * Selects items according to keyboard behaviours.
   * @param {CustomEvent} e
   * @private
   */
  onItemSelected_: function(e) {
    if (!e.detail.add)
      this.clearSelectedItems_();

    if (e.detail.range)
      this.selectRange_(e.detail.item);
    else
      this.selectItem_(e.detail.item);
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
  bookmarkNode.isSelectedItem = false;
  if (idToNodeMap)
    idToNodeMap[bookmarkNode.id] = bookmarkNode;

  if (bookmarkNode.url)
    return;

  bookmarkNode.isSelectedFolder = false;
  bookmarkNode.isOpen = true;
  for (var i = 0; i < bookmarkNode.children.length; i++)
    BookmarksStore.initNodes(bookmarkNode.children[i], idToNodeMap);
};
