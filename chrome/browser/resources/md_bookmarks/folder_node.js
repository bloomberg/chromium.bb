// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-folder-node',

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    itemId: {
      type: String,
      observer: 'updateFromStore',
    },

    depth: {
      type: Number,
      observer: 'depthChanged_',
    },

    /** @type {BookmarkNode} */
    item_: Object,

    /** @private */
    isClosed_: Boolean,

    /** @private */
    selectedFolder_: String,

    /** @private */
    searchActive_: Boolean,

    /** @private */
    isSelectedFolder_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      computed: 'computeIsSelected_(itemId, selectedFolder_, searchActive_)'
    },
  },

  listeners: {
    'keydown': 'onKeydown_',
  },

  /** @override */
  attached: function() {
    this.watch('item_', function(state) {
      return state.nodes[this.itemId];
    }.bind(this));
    this.watch('isClosed_', function(state) {
      return state.closedFolders.has(this.itemId);
    }.bind(this));
    this.watch('selectedFolder_', function(state) {
      return state.selectedFolder;
    });
    this.watch('searchActive_', function(state) {
      return bookmarks.util.isShowingSearch(state);
    });

    this.updateFromStore();
  },

  /** @return {HTMLElement} */
  getFocusTarget: function() {
    return this.$.container;
  },

  /** @return {HTMLElement} */
  getDropTarget: function() {
    return this.$.container;
  },

  /** @return {boolean} */
  isTopLevelFolder_: function() {
    return this.depth == 0;
  },

  /**
   * @private
   * @param {!Event} e
   */
  onKeydown_: function(e) {
    var direction = 0;
    var handled = true;
    // TODO(calamity): Handle left/right arrow keys.
    if (e.key == 'ArrowUp') {
      direction = -1;
    } else if (e.key == 'ArrowDown') {
      direction = 1;
    } else {
      handled = false;
    }

    if (direction)
      this.changeKeyboardSelection_(direction, this.root.activeElement);

    if (!handled)
      return;

    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * @private
   * @param {number} direction
   * @param {!HTMLElement} currentFocus
   */
  changeKeyboardSelection_: function(direction, currentFocus) {
    var newFocusFolderNode = null;
    var isChildFolderNodeFocused =
        currentFocus.tagName == 'BOOKMARKS-FOLDER-NODE';
    var reverse = direction == -1;

    // The current node's successor is its first child when open.
    if (!isChildFolderNodeFocused && !reverse && !this.isClosed_) {
      var children = this.getChildFolderNodes_();
      if (children.length)
        newFocusFolderNode = children[0];
    }

    if (isChildFolderNodeFocused) {
      // Get the next child folder node if a child is focused.
      if (!newFocusFolderNode) {
        newFocusFolderNode = this.getNextChild_(
            reverse,
            /** @type {!BookmarksFolderNodeElement} */ (currentFocus));
      }

      // The first child's predecessor is this node.
      if (!newFocusFolderNode && reverse)
        newFocusFolderNode = this;
    }

    // If there is no newly focused node, allow the parent to handle the change.
    if (!newFocusFolderNode) {
      if (this.itemId != ROOT_NODE_ID)
        this.getParentFolderNode_().changeKeyboardSelection_(direction, this);

      return;
    }

    // The root node is not navigable.
    if (newFocusFolderNode.itemId != ROOT_NODE_ID) {
      newFocusFolderNode.selectFolder_();
      newFocusFolderNode.getFocusTarget().focus();
    }
  },

  /**
   * Returns the next or previous visible bookmark node relative to |child|.
   * @private
   * @param {boolean} reverse
   * @param {!BookmarksFolderNodeElement} child
   * @return {BookmarksFolderNodeElement|null} Returns null if there is no child
   *     before/after |child|.
   */
  getNextChild_: function(reverse, child) {
    var newFocus = null;
    var children = this.getChildFolderNodes_();

    var index = children.indexOf(child);
    assert(index != -1);
    if (reverse) {
      // A child node's predecessor is either the previous child's last visible
      // descendant, or this node, which is its immediate parent.
      newFocus =
          index == 0 ? null : children[index - 1].getLastVisibleDescendant_();
    } else if (index < children.length - 1) {
      // A successor to a child is the next child.
      newFocus = children[index + 1];
    }

    return newFocus;
  },

  /**
   * Returns the immediate parent folder node, or null if there is none.
   * @private
   * @return {BookmarksFolderNodeElement|null}
   */
  getParentFolderNode_: function() {
    var parentFolderNode = this.parentNode;
    while (parentFolderNode &&
           parentFolderNode.tagName != 'BOOKMARKS-FOLDER-NODE') {
      parentFolderNode = parentFolderNode.parentNode || parentFolderNode.host;
    }
    return parentFolderNode || null;
  },

  /**
   * @private
   * @return {BookmarksFolderNodeElement}
   */
  getLastVisibleDescendant_: function() {
    var children = this.getChildFolderNodes_();
    if (this.isClosed_ || children.length == 0)
      return this;

    return children.pop().getLastVisibleDescendant_();
  },

  /**
   * @private
   * @return {string}
   */
  getFolderIcon_: function() {
    return this.isSelectedFolder_ ? 'bookmarks:folder-open' : 'cr:folder';
  },

  /** @private */
  selectFolder_: function() {
    this.dispatch(
        bookmarks.actions.selectFolder(this.item_.id, this.getState().nodes));
  },

  /**
   * @private
   * @return {!Array<!BookmarksFolderNodeElement>}
   */
  getChildFolderNodes_: function() {
    return Array.from(this.root.querySelectorAll('bookmarks-folder-node'));
  },

  /**
   * Occurs when the drop down arrow is tapped.
   * @private
   * @param {!Event} e
   */
  toggleFolder_: function(e) {
    this.dispatch(
        bookmarks.actions.changeFolderOpen(this.item_.id, this.isClosed_));
    e.stopPropagation();
  },

  /**
   * @private
   * @param {!Event} e
   */
  preventDefault_: function(e) {
    e.preventDefault();
  },

  /**
   * @private
   * @param {string} itemId
   * @param {string} selectedFolder
   * @return {boolean}
   */
  computeIsSelected_: function(itemId, selectedFolder, searchActive) {
    return itemId == selectedFolder && !searchActive;
  },

  /**
   * @private
   * @return {boolean}
   */
  hasChildFolder_: function() {
    return bookmarks.util.hasChildFolders(this.itemId, this.getState().nodes);
  },

  /** @private */
  depthChanged_: function() {
    this.style.setProperty('--node-depth', String(this.depth));
  },

  /**
   * @private
   * @return {number}
   */
  getChildDepth_: function() {
    return this.depth + 1;
  },

  /**
   * @param {string} itemId
   * @private
   * @return {boolean}
   */
  isFolder_: function(itemId) {
    return !this.getState().nodes[itemId].url;
  },

  /**
   * @private
   * @return {boolean}
   */
  isRootFolder_: function() {
    return this.itemId == ROOT_NODE_ID;
  },

  /**
   * @private
   * @return {string}
   */
  getTabIndex_: function() {
    return this.isSelectedFolder_ ? '0' : '';
  },
});
