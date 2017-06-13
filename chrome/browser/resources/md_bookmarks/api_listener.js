// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Listener functions which translate events from the
 * chrome.bookmarks API into actions to modify the local page state.
 */

cr.define('bookmarks.ApiListener', function() {
  /** @param {Action} action */
  function dispatch(action) {
    bookmarks.Store.getInstance().dispatch(action);
  }

  /**
   * @param {string} id
   * @param {{title: string, url: (string|undefined)}} changeInfo
   */
  function onBookmarkChanged(id, changeInfo) {
    dispatch(bookmarks.actions.editBookmark(id, changeInfo));
  }

  /**
   * @param {string} id
   * @param {BookmarkTreeNode} treeNode
   */
  function onBookmarkCreated(id, treeNode) {
    dispatch(bookmarks.actions.createBookmark(id, treeNode));
  }

  /**
   * @param {string} id
   * @param {{parentId: string, index: number}} removeInfo
   */
  function onBookmarkRemoved(id, removeInfo) {
    var nodes = bookmarks.Store.getInstance().data.nodes;
    dispatch(bookmarks.actions.removeBookmark(
        id, removeInfo.parentId, removeInfo.index, nodes));
  }

  /**
   * @param {string} id
   * @param {{
   *    parentId: string,
   *    index: number,
   *    oldParentId: string,
   *    oldIndex: number
   * }} moveInfo
   */
  function onBookmarkMoved(id, moveInfo) {
    dispatch(bookmarks.actions.moveBookmark(
        id, moveInfo.parentId, moveInfo.index, moveInfo.oldParentId,
        moveInfo.oldIndex));
  }

  /**
   * @param {string} id
   * @param {{childIds: !Array<string>}} reorderInfo
   */
  function onChildrenReordered(id, reorderInfo) {
    dispatch(bookmarks.actions.reorderChildren(id, reorderInfo.childIds));
  }

  /**
   * Pauses the Created handler during an import. The imported nodes will all be
   * loaded at once when the import is finished.
   */
  function onImportBegan() {
    chrome.bookmarks.onCreated.removeListener(onBookmarkCreated);
  }

  function onImportEnded() {
    chrome.bookmarks.getTree(function(results) {
      dispatch(bookmarks.actions.refreshNodes(
          bookmarks.util.normalizeNodes(results[0])));
    });
    chrome.bookmarks.onCreated.addListener(onBookmarkCreated);
  }

  /**
   * @param {IncognitoAvailability} availability
   */
  function onIncognitoAvailabilityChanged(availability) {
    dispatch(bookmarks.actions.setIncognitoAvailability(availability));
  }

  /**
   * @param {boolean} canEdit
   */
  function onCanEditBookmarksChanged(canEdit) {
    dispatch(bookmarks.actions.setCanEditBookmarks(canEdit));
  }

  function init() {
    chrome.bookmarks.onChanged.addListener(onBookmarkChanged);
    chrome.bookmarks.onChildrenReordered.addListener(onChildrenReordered);
    chrome.bookmarks.onCreated.addListener(onBookmarkCreated);
    chrome.bookmarks.onMoved.addListener(onBookmarkMoved);
    chrome.bookmarks.onRemoved.addListener(onBookmarkRemoved);
    chrome.bookmarks.onImportBegan.addListener(onImportBegan);
    chrome.bookmarks.onImportEnded.addListener(onImportEnded);

    cr.sendWithPromise('getIncognitoAvailability')
        .then(onIncognitoAvailabilityChanged);
    cr.addWebUIListener(
        'incognito-availability-changed', onIncognitoAvailabilityChanged);

    cr.sendWithPromise('getCanEditBookmarks').then(onCanEditBookmarksChanged);
    cr.addWebUIListener(
        'can-edit-bookmarks-changed', onCanEditBookmarksChanged);
  }

  return {
    init: init,
  };
});
