// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module of functions which produce a new page state in response
 * to an action. Reducers (in the same sense as Array.prototype.reduce) must be
 * pure functions: they must not modify existing state objects, or make any API
 * calls.
 */

cr.define('bookmarks', function() {
  var SelectionState = {};

  /**
   * @param {SelectionState} selectionState
   * @param {Action} action
   * @return {SelectionState}
   */
  SelectionState.selectItems = function(selectionState, action) {
    var newItems = {};
    if (action.add)
      Object.assign(newItems, selectionState.items);

    action.items.forEach(function(id) {
      newItems[id] = true;
    });

    return /** @type {SelectionState} */ (Object.assign({}, selectionState, {
      items: newItems,
      anchor: action.anchor,
    }));
  };

  /**
   * @param {SelectionState} selectionState
   * @return {SelectionState}
   */
  SelectionState.deselectAll = function(selectionState) {
    return {
      items: {},
      anchor: null,
    };
  };

  /**
   * @param {SelectionState} selection
   * @param {Action} action
   * @return {SelectionState}
   */
  SelectionState.updateSelection = function(selection, action) {
    switch (action.name) {
      case 'clear-search':
      case 'finish-search':
      case 'select-folder':
        return SelectionState.deselectAll(selection);
      case 'select-items':
        return SelectionState.selectItems(selection, action);
    }
    return selection;
  };

  var SearchState = {};

  /**
   * @param {SearchState} search
   * @param {Action} action
   * @return {SearchState}
   */
  SearchState.startSearch = function(search, action) {
    return {
      term: action.term,
      inProgress: true,
      results: [],
    };
  };

  /**
   * @param {SearchState} search
   * @param {Action} action
   * @return {SearchState}
   */
  SearchState.finishSearch = function(search, action) {
    return /** @type {SearchState} */ (Object.assign({}, search, {
      inProgress: false,
      results: action.results,
    }));
  };

  /** @return {SearchState} */
  SearchState.clearSearch = function() {
    return {
      term: '',
      inProgress: false,
      results: [],
    };
  };

  /**
   * @param {SearchState} search
   * @param {Action} action
   * @return {SearchState}
   */
  SearchState.updateSearch = function(search, action) {
    switch (action.name) {
      case 'start-search':
        return SearchState.startSearch(search, action);
      case 'select-folder':
      case 'clear-search':
        return SearchState.clearSearch();
      case 'finish-search':
        return SearchState.finishSearch(search, action);
      default:
        return search;
    }
  };

  var NodeState = {};

  /**
   * @param {NodeList} nodes
   * @param {string} id
   * @param {function(BookmarkNode):BookmarkNode} callback
   * @return {NodeList}
   */
  NodeState.modifyNode_ = function(nodes, id, callback) {
    var nodeModification = {};
    nodeModification[id] = callback(nodes[id]);
    return Object.assign({}, nodes, nodeModification);
  };

  /**
   * @param {NodeList} nodes
   * @param {Action} action
   * @return {NodeList}
   */
  NodeState.editBookmark = function(nodes, action) {
    // Do not allow folders to change URL (making them no longer folders).
    if (!nodes[action.id].url && action.changeInfo.url)
      delete action.changeInfo.url;

    return NodeState.modifyNode_(nodes, action.id, function(node) {
      return /** @type {BookmarkNode} */ (
          Object.assign({}, node, action.changeInfo));
    });
  };

  /**
   * @param {NodeList} nodes
   * @param {Action} action
   * @return {NodeList}
   */
  NodeState.removeBookmark = function(nodes, action) {
    return NodeState.modifyNode_(nodes, action.parentId, function(node) {
      var newChildren = node.children.slice();
      newChildren.splice(action.index, 1);
      return /** @type {BookmarkNode} */ (
          Object.assign({}, node, {children: newChildren}));
    });
  };

  /**
   * @param {NodeList} nodes
   * @param {Action} action
   * @return {NodeList}
   */
  NodeState.moveBookmark = function(nodes, action) {
    var nodeModifications = {};
    var id = action.id;

    // Change node's parent.
    nodeModifications[id] =
        Object.assign({}, nodes[id], {parentId: action.parentId});

    // Remove from old parent.
    var oldParentId = action.oldParentId;
    var oldParentChildren = nodes[oldParentId].children.slice();
    oldParentChildren.splice(action.oldIndex, 1);
    nodeModifications[oldParentId] =
        Object.assign({}, nodes[oldParentId], {children: oldParentChildren});

    // Add to new parent.
    var parentId = action.parentId;
    var parentChildren = oldParentId == parentId ?
        oldParentChildren :
        nodes[parentId].children.slice();
    parentChildren.splice(action.index, 0, action.id);
    nodeModifications[parentId] =
        Object.assign({}, nodes[parentId], {children: parentChildren});

    return Object.assign({}, nodes, nodeModifications);
  };

  /**
   * @param {NodeList} nodes
   * @param {Action} action
   * @return {NodeList}
   */
  NodeState.updateNodes = function(nodes, action) {
    switch (action.name) {
      case 'edit-bookmark':
        return NodeState.editBookmark(nodes, action);
      case 'remove-bookmark':
        return NodeState.removeBookmark(nodes, action);
      case 'move-bookmark':
        return NodeState.moveBookmark(nodes, action);
      case 'refresh-nodes':
        return action.nodes;
      default:
        return nodes;
    }
  };

  var SelectedFolderState = {};

  /**
   * @param {NodeList} nodes
   * @param {string} ancestorId
   * @param {string} childId
   * @return {boolean}
   */
  SelectedFolderState.isAncestorOf = function(nodes, ancestorId, childId) {
    var currentId = childId;
    // Work upwards through the tree from child.
    while (currentId) {
      if (currentId == ancestorId)
        return true;
      currentId = nodes[currentId].parentId;
    }
    return false;
  };

  /**
   * @param {?string} selectedFolder
   * @param {Action} action
   * @param {NodeList} nodes
   * @return {?string}
   */
  SelectedFolderState.updateSelectedFolder = function(
      selectedFolder, action, nodes) {
    // TODO(tsergeant): It should not be possible to select a non-folder.
    // TODO(tsergeant): Select parent folder when selected folder is deleted.
    switch (action.name) {
      case 'select-folder':
        return action.id;
      case 'change-folder-open':
        // When hiding the selected folder by closing its ancestor, select
        // that ancestor instead.
        if (!action.open && selectedFolder &&
            SelectedFolderState.isAncestorOf(
                nodes, action.id, selectedFolder)) {
          return action.id;
        }
        return selectedFolder;
      case 'finish-search':
        return null;
      case 'clear-search':
        // TODO(tsergeant): Return to the folder that was selected before the
        // search.
        return nodes['0'].children[0];
      default:
        return selectedFolder;
    }
  };

  var ClosedFolderState = {};

  /**
   * @param {ClosedFolderState} closedFolders
   * @param {string|undefined} id
   * @param {NodeList} nodes
   * @return {ClosedFolderState}
   */
  ClosedFolderState.openFolderAndAncestors = function(
      closedFolders, id, nodes) {
    var modifications = {};
    var currentId = id;
    while (currentId) {
      if (closedFolders[currentId]) {
        modifications[currentId] = false;
      }
      currentId = nodes[currentId].parentId;
    }

    return Object.assign({}, closedFolders, modifications);
  };

  /**
   * @param {ClosedFolderState} closedFolders
   * @param {Action} action
   * @return {ClosedFolderState}
   */
  ClosedFolderState.changeFolderOpen = function(closedFolders, action) {
    var closed = !action.open;
    var modification = {};
    modification[action.id] = closed;

    return Object.assign({}, closedFolders, modification);
  };

  /**
   * @param {ClosedFolderState} closedFolders
   * @param {Action} action
   * @param {NodeList} nodes
   * @return {ClosedFolderState}
   */
  ClosedFolderState.updateClosedFolders = function(
      closedFolders, action, nodes) {
    switch (action.name) {
      case 'change-folder-open':
        return ClosedFolderState.changeFolderOpen(closedFolders, action);
      case 'select-folder':
        return ClosedFolderState.openFolderAndAncestors(
            closedFolders, nodes[action.id].parentId, nodes);
      case 'move-bookmark':
        if (!nodes[action.id].children)
          return closedFolders;

        return ClosedFolderState.openFolderAndAncestors(
            closedFolders, action.parentId, nodes);
      default:
        return closedFolders;
    };
  };

  /**
   * Root reducer for the Bookmarks page. This is called by the store in
   * response to an action, and the return value is used to update the UI.
   * @param {!BookmarksPageState} state
   * @param {Action} action
   * @return {!BookmarksPageState}
   */
  function reduceAction(state, action) {
    return {
      nodes: NodeState.updateNodes(state.nodes, action),
      selectedFolder: SelectedFolderState.updateSelectedFolder(
          state.selectedFolder, action, state.nodes),
      closedFolders: ClosedFolderState.updateClosedFolders(
          state.closedFolders, action, state.nodes),
      search: SearchState.updateSearch(state.search, action),
      selection: SelectionState.updateSelection(state.selection, action),
    };
  }

  return {
    reduceAction: reduceAction,
    ClosedFolderState: ClosedFolderState,
    NodeState: NodeState,
    SearchState: SearchState,
    SelectedFolderState: SelectedFolderState,
    SelectionState: SelectionState,
  };
});
