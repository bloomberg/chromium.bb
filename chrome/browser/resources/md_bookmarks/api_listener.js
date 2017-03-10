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
    bookmarks.Store.getInstance().handleAction(action);
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
   * @param {{parentId: string, index: number}} removeInfo
   */
  function onBookmarkRemoved(id, removeInfo) {
    dispatch(bookmarks.actions.removeBookmark(
        id, removeInfo.parentId, removeInfo.index));
  }

  function onImportBegan() {
    // TODO(rongjie): pause onCreated once this event is used.
  }

  function onImportEnded() {
    chrome.bookmarks.getTree(function(results) {
      dispatch(bookmarks.actions.refreshNodes(
          bookmarks.util.normalizeNodes(results[0])));
    });
  }

  function init() {
    chrome.bookmarks.onChanged.addListener(onBookmarkChanged);
    chrome.bookmarks.onRemoved.addListener(onBookmarkRemoved);
    chrome.bookmarks.onImportBegan.addListener(onImportBegan);
    chrome.bookmarks.onImportEnded.addListener(onImportEnded);
  }

  return {
    init: init,
  };
});
