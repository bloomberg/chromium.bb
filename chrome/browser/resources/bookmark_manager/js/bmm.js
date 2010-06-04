// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

  return {
    contains: contains,
    isFolder: isFolder,
    loadSubtree: loadSubtree,
    loadTree: loadTree
  };
});
