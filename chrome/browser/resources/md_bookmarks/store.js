// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-store',

  properties: {
    /** @type {BookmarkTreeNode} */
    rootNode: {
      type: Object,
      notify: true,
    },

    selectedId: {
      type: String,
      observer: 'updateSelectedNode_',
      notify: true,
    },

    /** @type {BookmarkTreeNode} */
    selectedNode: {
      type: Object,
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
  },

  /**
   * @param {BookmarkTreeNode} rootNode
   * @private
   */
  setupStore_: function(rootNode) {
    this.rootNode = rootNode;
    this.idToNodeMap_ = {};
    this.rootNode.path = 'rootNode';
    this.initNodes_(this.rootNode);
    this.fire('selected-folder-changed', this.rootNode.children[0].id);
  },

  /**
   * Selects the folder specified by the event and deselects the previously
   * selected folder.
   * @param {CustomEvent} e
   * @private
   */
  onSelectedFolderChanged_: function(e) {
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

  /**
   * @param {BookmarkTreeNode} folder
   * @private
   * @return {boolean}
   */
  isAncestorOfSelected_: function(folder) {
    return this.selectedNode.path.startsWith(folder.path);
  },

  /** @private */
  updateSelectedNode_: function() {
    var selectedNode = this.idToNodeMap_[this.selectedId];
    this.linkPaths('selectedNode', selectedNode.path);
    this._setSelectedNode(selectedNode);
  },

  /**
   * Initializes the nodes in the bookmarks tree as follows:
   * - Populates |idToNodeMap_| with a mapping of all node ids to their
   *   corresponding BookmarkTreeNode.
   * - Stores the path from the store to a node inside the node.
   * - Sets all the nodes to not selected and open by default.
   * @param {BookmarkTreeNode} bookmarkNode
   * @private
   */
  initNodes_: function(bookmarkNode) {
    this.idToNodeMap_[bookmarkNode.id] = bookmarkNode;
    if (bookmarkNode.url)
      return;

    bookmarkNode.isSelected = false;
    bookmarkNode.isOpen = true;
    for (var i = 0; i < bookmarkNode.children.length; i++) {
      bookmarkNode.children[i].path = bookmarkNode.path + '.children.' + i;
      this.initNodes_(bookmarkNode.children[i]);
    }
  },
});
