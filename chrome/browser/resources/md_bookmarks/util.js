// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for the Bookmarks page.
 */

cr.define('bookmarks.util', function() {
  /**
   * Returns the list of bookmark IDs to be displayed in the UI, taking into
   * account search and the currently selected folder.
   * @param {!BookmarksPageState} state
   * @return {!Array<string>}
   */
  function getDisplayedList(state) {
    if (!isShowingSearch(state))
      return assert(state.nodes[state.selectedFolder].children);

    return state.search.results;
  }

  /**
   * @param {BookmarkTreeNode} treeNode
   * @return {BookmarkNode}
   */
  function normalizeNode(treeNode) {
    var node = Object.assign({}, treeNode);
    // Node index is not necessary and not kept up-to-date. Remove it from the
    // data structure so we don't accidentally depend on the incorrect
    // information.
    delete node.index;

    if (!('url' in node)) {
      // The onCreated API listener returns folders without |children| defined.
      node.children = (node.children || []).map(function(child) {
        return child.id;
      });
    }
    return /** @type {BookmarkNode} */ (node);
  }

  /**
   * @param {BookmarkTreeNode} rootNode
   * @return {NodeMap}
   */
  function normalizeNodes(rootNode) {
    /** @type {NodeMap} */
    var nodeMap = {};
    var stack = [];
    stack.push(rootNode);

    while (stack.length > 0) {
      var node = stack.pop();
      nodeMap[node.id] = normalizeNode(node);
      if (!node.children)
        continue;

      node.children.forEach(function(child) {
        stack.push(child);
      });
    }

    return nodeMap;
  }

  /** @return {!BookmarksPageState} */
  function createEmptyState() {
    return {
      nodes: {},
      selectedFolder: '0',
      closedFolders: new Set(),
      search: {
        term: '',
        inProgress: false,
        results: [],
      },
      selection: {
        items: new Set(),
        anchor: null,
      },
    };
  }

  /**
   * @param {BookmarksPageState} state
   * @return {boolean}
   */
  function isShowingSearch(state) {
    return !!state.search.term && !state.search.inProgress;
  }

  /**
   * @param {string} id
   * @param {NodeMap} nodes
   * @return {boolean}
   */
  function hasChildFolders(id, nodes) {
    var children = nodes[id].children;
    for (var i = 0; i < children.length; i++) {
      if (nodes[children[i]].children)
        return true;
    }
    return false;
  }

  /**
   * Get all descendants of a node, including the node itself.
   * @param {NodeMap} nodes
   * @param {string} baseId
   * @return {!Set<string>}
   */
  function getDescendants(nodes, baseId) {
    var descendants = new Set();
    var stack = [];
    stack.push(baseId);

    while (stack.length > 0) {
      var id = stack.pop();
      var node = nodes[id];

      if (!node)
        continue;

      descendants.add(id);

      if (!node.children)
        continue;

      node.children.forEach(function(childId) {
        stack.push(childId);
      });
    }

    return descendants;
  }

  /**
   * @param {!Object<string, T>} map
   * @param {!Set<string>} ids
   * @return {!Object<string, T>}
   * @template T
   */
  function removeIdsFromMap(map, ids) {
    var newMap = Object.assign({}, map);
    ids.forEach(function(id) {
      delete newMap[id];
    });
    return newMap;
  }

  /**
   * @param {!Set<string>} set
   * @param {!Set<string>} ids
   * @return {!Set<string>}
   */
  function removeIdsFromSet(set, ids) {
    var difference = new Set(set);
    ids.forEach(function(id) {
      difference.delete(id);
    });
    return difference;
  }

  return {
    createEmptyState: createEmptyState,
    getDescendants: getDescendants,
    getDisplayedList: getDisplayedList,
    hasChildFolders: hasChildFolders,
    isShowingSearch: isShowingSearch,
    normalizeNode: normalizeNode,
    normalizeNodes: normalizeNodes,
    removeIdsFromMap: removeIdsFromMap,
    removeIdsFromSet: removeIdsFromSet,
  };
});
