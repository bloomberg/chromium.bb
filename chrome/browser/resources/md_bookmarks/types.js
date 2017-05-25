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
 *   nodes: NodeMap,
 *   selectedFolder: string,
 *   closedFolders: ClosedFolderState,
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

// TODO(calamity): Remove once
// https://github.com/google/closure-compiler/pull/2495 is merged.
Polymer.ArraySplice = {};

/**
 * Returns an array of splice records indicating the minimum edits required
 * to transform the `previous` array into the `current` array.
 *
 * Splice records are ordered by index and contain the following fields:
 * - `index`: index where edit started
 * - `removed`: array of removed items from this index
 * - `addedCount`: number of items added at this index
 *
 * This function is based on the Levenshtein "minimum edit distance"
 * algorithm. Note that updates are treated as removal followed by addition.
 *
 * The worst-case time complexity of this algorithm is `O(l * p)`
 *   l: The length of the current array
 *   p: The length of the previous array
 *
 * However, the worst-case complexity is reduced by an `O(n)` optimization
 * to detect any shared prefix & suffix between the two arrays and only
 * perform the more expensive minimum edit distance calculation over the
 * non-shared portions of the arrays.
 *
 * @param {Array} current The "changed" array for which splices will be
 * calculated.
 * @param {Array} previous The "unchanged" original array to compare
 * `current` against to determine the splices.
 * @return {Array} Returns an array of splice record objects. Each of these
 * contains: `index` the location where the splice occurred; `removed`
 * the array of removed items from this location; `addedCount` the number
 * of items added at this location.
 */
Polymer.ArraySplice.calculateSplices = function(current, previous) {};
