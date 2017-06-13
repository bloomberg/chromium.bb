// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for MD Bookmarks.
 */

/**
 * A normalized version of chrome.bookmarks.BookmarkTreeNode.
 * @typedef {{
 *   id: string,
 *   parentId: (string|undefined),
 *   url: (string|undefined),
 *   title: string,
 *   dateAdded: (number|undefined),
 *   dateGroupModified: (number|undefined),
 *   unmodifiable: (string|undefined),
 *   children: (!Array<string>|undefined),
 * }}
 */
var BookmarkNode;

/**
 * @typedef {!Object<string, BookmarkNode>}
 */
var NodeMap;

/**
 * @typedef {{
 *   items: !Set<string>,
 *   anchor: ?string,
 * }}
 *
 * |items| is used as a set and all values in the map are true.
 */
var SelectionState;

/**
 * @typedef {{
 *   term: string,
 *   inProgress: boolean,
 *   results: !Array<string>,
 * }}
 */
var SearchState;

/** @typedef {!Set<string>} */
var ClosedFolderState;

/**
 * @typedef {{
 *   canEdit: boolean,
 *   incognitoAvailability: IncognitoAvailability,
 * }}
 */
var PreferencesState;

/**
 * @typedef {{
 *   nodes: NodeMap,
 *   selectedFolder: string,
 *   closedFolders: ClosedFolderState,
 *   prefs: PreferencesState,
 *   search: SearchState,
 *   selection: SelectionState,
 * }}
 */
var BookmarksPageState;

/** @typedef {{name: string}} */
var Action;

/** @typedef {function(function(?Action))} */
var DeferredAction;

/** @typedef {{element: BookmarkElement, position: DropPosition}} */
var DropDestination;

/**
 * @record
 */
function BookmarkElement() {}

/** @type {string} */
BookmarkElement.itemId;

/** @return {HTMLElement} */
BookmarkElement.getDropTarget = function() {};

/** @constructor */
function DragData() {
  /** @type {Array<BookmarkTreeNode>} */
  this.elements = null;

  /** @type {boolean} */
  this.sameProfile = false;
}

/** @interface */
function StoreObserver() {}

/** @param {!BookmarksPageState} newState */
StoreObserver.prototype.onStateChanged = function(newState) {};
