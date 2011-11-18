// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('bmm', function() {
  const Promise = cr.Promise;

  /**
   * Whether a node contains another node.
   * @param {!BookmarkTreeNode} parent
   * @param {!BookmarkTreeNode} descendant
   * @return {boolean} Whether the parent contains the descendant.
   */
  function contains(parent, descendant) {
    if (descendant.parentId == parent.id)
      return true;
    // the bmm.treeLookup contains all folders
    var parentTreeItem = bmm.treeLookup[descendant.parentId];
    if (!parentTreeItem || !parentTreeItem.bookmarkNode)
      return false;
    return this.contains(parent, parentTreeItem.bookmarkNode);
  }

  /**
   * @param {!BookmarkTreeNode} node The node to test.
   * @return {boolean} Whether a bookmark node is a folder.
   */
  function isFolder(node) {
    return !('url' in node);
  }

  var loadingPromises = {};

  /**
   * Loads a subtree of the bookmark tree and returns a {@code cr.Promise} that
   * will be fulfilled when done. This reuses multiple loads so that we do not
   * load the same subtree more than once at the same time.
   * @return {!cr.Promise} The future promise for the load.
   */
  function loadSubtree(id) {
    var p = new Promise;
    if (!(id in loadingPromises)) {
      loadingPromises[id] = new Promise;
      chrome.experimental.bookmarkManager.getSubtree(id, false,
                                                     function(nodes) {
        loadingPromises[id].value = nodes && nodes[0];
        delete loadingPromises[id];
      });
    }
    loadingPromises[id].addListener(function(n) {
      p.value = n;
    });
    return p;
  }

  /**
   * Loads the entire bookmark tree and returns a {@code cr.Promise} that will
   * be fulfilled when done. This reuses multiple loads so that we do not load
   * the same tree more than once at the same time.
   * @return {!cr.Promise} The future promise for the load.
   */
  function loadTree() {
    return loadSubtree('');
  }

  var bookmarkCache = {
    /**
     * Removes the cached item from both the list and tree lookups.
     */
    remove: function(id) {
      var treeItem = bmm.treeLookup[id];
      if (treeItem) {
        var items = treeItem.items; // is an HTMLCollection
        for (var i = 0; i < items.length; ++i) {
          var item = items[i];
          var bookmarkNode = item.bookmarkNode;
          delete bmm.treeLookup[bookmarkNode.id];
        }
        delete bmm.treeLookup[id];
      }
    },

    /**
     * Updates the underlying bookmark node for the tree items and list items by
     * querying the bookmark backend.
     * @param {string} id The id of the node to update the children for.
     * @param {Function=} opt_f A funciton to call when done.
     */
    updateChildren: function(id, opt_f) {
      function updateItem(bookmarkNode) {
        var treeItem = bmm.treeLookup[bookmarkNode.id];
        if (treeItem) {
          treeItem.bookmarkNode = bookmarkNode;
        }
      }

      chrome.bookmarks.getChildren(id, function(children) {
        if (children)
          children.forEach(updateItem);

        if (opt_f)
          opt_f(children);
      });
    }
  };

  /**
   * Called when the title of a bookmark changes.
   * @param {string} id
   * @param {!Object} changeInfo
   */
  function handleBookmarkChanged(id, changeInfo) {
    if (bmm.tree)
      bmm.tree.handleBookmarkChanged(id, changeInfo);
    if (bmm.list)
      bmm.list.handleBookmarkChanged(id, changeInfo);
  }

  /**
   * Callback for when the user reorders by title.
   * @param {string} id The id of the bookmark folder that was reordered.
   * @param {!Object} reorderInfo The information about how the items where
   *     reordered.
   */
  function handleChildrenReordered(id, reorderInfo) {
    if (bmm.tree)
      bmm.tree.handleChildrenReordered(id, changeInfo);
    if (bmm.list)
      bmm.list.handleChildrenReordered(id, changeInfo);
    bookmarkCache.updateChildren(id);
  }

  /**
   * Callback for when a bookmark node is created.
   * @param {string} id The id of the newly created bookmark node.
   * @param {!Object} bookmarkNode The new bookmark node.
   */
  function handleCreated(id, bookmarkNode) {
    if (bmm.list)
      bmm.list.handleCreated(id, bookmarkNode);
    if (bmm.tree)
      bmm.tree.handleCreated(id, bookmarkNode);
    bookmarkCache.updateChildren(bookmarkNode.parentId);
  }

  /**
   * Callback for when a bookmark node is moved.
   * @param {string} id The id of the moved bookmark node.
   * @param {!Object} moveInfo The information about move.
   */
  function handleMoved(id, moveInfo) {
    if (bmm.list)
      bmm.list.handleMoved(id, moveInfo);
    if (bmm.tree)
      bmm.tree.handleMoved(id, moveInfo);

    bookmarkCache.updateChildren(moveInfo.parentId);
    if (moveInfo.parentId != moveInfo.oldParentId)
      bookmarkCache.updateChildren(moveInfo.oldParentId);
  }

  /**
   * Callback for when a bookmark node is removed.
   * @param {string} id The id of the removed bookmark node.
   * @param {!Object} bookmarkNode The information about removed.
   */
  function handleRemoved(id, removeInfo) {
    if (bmm.list)
      bmm.list.handleRemoved(id, removeInfo);
    if (bmm.tree)
      bmm.tree.handleRemoved(id, removeInfo);

    bookmarkCache.updateChildren(removeInfo.parentId);
    bookmarkCache.remove(id);
  }

  /**
   * Callback for when importing bookmark is started.
   */
  function handleImportBegan() {
    chrome.bookmarks.onCreated.removeListener(handleCreated);
    chrome.bookmarks.onChanged.removeListener(handleBookmarkChanged);
  }

  /**
   * Callback for when importing bookmark node is finished.
   */
  function handleImportEnded() {
    // When importing is done we reload the tree and the list.

    function f() {
      bmm.tree.removeEventListener('load', f);

      chrome.bookmarks.onCreated.addListener(handleCreated);
      chrome.bookmarks.onChanged.addListener(handleBookmarkChanged);

      if (!bmm.list)
        return;

      if (bmm.list.selectImportedFolder) {
        var otherBookmarks = bmm.tree.items[1].items;
        var importedFolder = otherBookmarks[otherBookmarks.length - 1];
        navigateTo(importedFolder.bookmarkId)
        bmm.list.selectImportedFolder = false
      } else {
        bmm.list.reload();
      }
    }

    if (bmm.tree) {
      bmm.treeaddEventListener('load', f);
      bmm.tree.reload();
    }
  }

  /**
   * Adds the listeners for the bookmark model change events.
   */
  function addBookmarkModelListeners() {
    chrome.bookmarks.onChanged.addListener(handleBookmarkChanged);
    chrome.bookmarks.onChildrenReordered.addListener(handleChildrenReordered);
    chrome.bookmarks.onCreated.addListener(handleCreated);
    chrome.bookmarks.onMoved.addListener(handleMoved);
    chrome.bookmarks.onRemoved.addListener(handleRemoved);
    chrome.bookmarks.onImportBegan.addListener(handleImportBegan);
    chrome.bookmarks.onImportEnded.addListener(handleImportEnded);
  };

  return {
    contains: contains,
    isFolder: isFolder,
    loadSubtree: loadSubtree,
    loadTree: loadTree,
    addBookmarkModelListeners: addBookmarkModelListeners
  };
});
