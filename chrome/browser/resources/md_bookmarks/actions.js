// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module for functions which produce action objects. These are
 * listed in one place to document available actions and their parameters.
 */

cr.define('bookmarks.actions', function() {
  /**
   * @param {string} id
   * @param {{title: string, url: (string|undefined)}} changeInfo
   * @return {!Action}
   */
  function editBookmark(id, changeInfo) {
    return {
      name: 'edit-bookmark',
      id: id,
      changeInfo: changeInfo,
    };
  }

  /**
   * @param {string} id
   * @param {string} parentId
   * @param {number} index
   * @return {!Action}
   */
  function removeBookmark(id, parentId, index) {
    return {
      name: 'remove-bookmark',
      id: id,
      parentId: parentId,
      index: index,
    };
  }

  /**
   * @param {NodeList} nodeMap
   * @return {!Action}
   */
  function refreshNodes(nodeMap) {
    return {
      name: 'refresh-nodes',
      nodes: nodeMap,
    };
  };

  /**
   * @param {string} id
   * @return {!Action}
   */
  function selectFolder(id) {
    assert(id != '0', 'Cannot select root folder');
    return {
      name: 'select-folder',
      id: id,
    };
  }

  /**
   * @param {string} id
   * @param {boolean} open
   * @return {!Action}
   */
  function changeFolderOpen(id, open) {
    return {
      name: 'change-folder-open',
      id: id,
      open: open,
    };
  }

  /** @return {!Action} */
  function clearSearch() {
    return {
      name: 'clear-search',
    };
  }

  /**
   * @param {string} term
   * @return {!Action}
   */
  function setSearchTerm(term) {
    if (!term)
      return clearSearch();

    return {
      name: 'start-search',
      term: term,
    };
  }

  /**
   * @param {!Array<string>} ids
   * @return {!Action}
   */
  function setSearchResults(ids) {
    return {
      name: 'finish-search',
      results: ids,
    };
  }

  return {
    changeFolderOpen: changeFolderOpen,
    clearSearch: clearSearch,
    editBookmark: editBookmark,
    refreshNodes: refreshNodes,
    removeBookmark: removeBookmark,
    selectFolder: selectFolder,
    setSearchResults: setSearchResults,
    setSearchTerm: setSearchTerm,
  };
});
