// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-folder-node',

  behaviors: [
    bookmarks.MouseFocusBehavior,
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

    if (this.isSelectedFolder_) {
      this.async(function() {
        this.scrollIntoViewIfNeeded();
      });
    }
  },

  /**
   * Overriden from bookmarks.MouseFocusBehavior.
   * @return {!HTMLElement}
   */
  getFocusTarget: function() {
    return this.$.container;
  },

  /** @return {HTMLElement} */
  getDropTarget: function() {
    return this.$.container;
  },

  /**
   * @private
   * @param {!Event} e
   */
  onKeydown_: function(e) {
    var yDirection = 0;
    var xDirection = 0;
    var handled = true;
    if (e.key == 'ArrowUp') {
      yDirection = -1;
    } else if (e.key == 'ArrowDown') {
      yDirection = 1;
    } else if (e.key == 'ArrowLeft') {
      xDirection = -1;
    } else if (e.key == 'ArrowRight') {
      xDirection = 1;
    } else {
      handled = false;
    }

    if (this.getComputedStyleValue('direction') == 'rtl')
      xDirection *= -1;

    this.changeKeyboardSelection_(
        xDirection, yDirection, this.root.activeElement);

    if (!handled) {
      handled = bookmarks.CommandManager.getInstance().handleKeyEvent(
          e, new Set([this.itemId]));
    }

    if (!handled)
      return;

    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * @private
   * @param {number} xDirection
   * @param {number} yDirection
   * @param {!HTMLElement} currentFocus
   */
  changeKeyboardSelection_: function(xDirection, yDirection, currentFocus) {
    var newFocusFolderNode = null;
    var isChildFolderNodeFocused =
        currentFocus.tagName == 'BOOKMARKS-FOLDER-NODE';

    if (xDirection == 1) {
      // The right arrow opens a folder if closed and goes to the first child
      // otherwise.
      if (this.hasChildFolder_()) {
        if (this.isClosed_) {
          this.dispatch(
              bookmarks.actions.changeFolderOpen(this.item_.id, true));
        } else {
          yDirection = 1;
        }
      }
    } else if (xDirection == -1) {
      // The left arrow closes a folder if open and goes to the parent
      // otherwise.
      if (this.hasChildFolder_() && !this.isClosed_) {
        this.dispatch(bookmarks.actions.changeFolderOpen(this.item_.id, false));
      } else {
        var parentFolderNode = this.getParentFolderNode_();
        if (parentFolderNode.itemId != ROOT_NODE_ID) {
          parentFolderNode.selectFolder_();
          parentFolderNode.getFocusTarget().focus();
        }
      }
    }

    if (!yDirection)
      return;

    // The current node's successor is its first child when open.
    if (!isChildFolderNodeFocused && yDirection == 1 && !this.isClosed_) {
      var children = this.getChildFolderNodes_();
      if (children.length)
        newFocusFolderNode = children[0];
    }

    if (isChildFolderNodeFocused) {
      // Get the next child folder node if a child is focused.
      if (!newFocusFolderNode) {
        newFocusFolderNode = this.getNextChild_(
            yDirection == -1,
            /** @type {!BookmarksFolderNodeElement} */ (currentFocus));
      }

      // The first child's predecessor is this node.
      if (!newFocusFolderNode && yDirection == -1)
        newFocusFolderNode = this;
    }

    // If there is no newly focused node, allow the parent to handle the change.
    if (!newFocusFolderNode) {
      if (this.itemId != ROOT_NODE_ID)
        this.getParentFolderNode_().changeKeyboardSelection_(
            0, yDirection, this);

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
    if (!this.isSelectedFolder_) {
      this.dispatch(
          bookmarks.actions.selectFolder(this.itemId, this.getState().nodes));
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onContextMenu_: function(e) {
    e.preventDefault();
    this.selectFolder_();
    bookmarks.CommandManager.getInstance().openCommandMenuAtPosition(
        e.clientX, e.clientY, new Set([this.itemId]));
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
        bookmarks.actions.changeFolderOpen(this.itemId, this.isClosed_));
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
    return this.isSelectedFolder_ ? '0' : '-1';
  },
});
