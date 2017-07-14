// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of valid drop locations relative to an element. These are
 * bit masks to allow combining multiple locations in a single value.
 * @enum {number}
 * @const
 */
var DropPosition = {
  NONE: 0,
  ABOVE: 1,
  ON: 2,
  BELOW: 4,
};

/**
 * @enum {string}
 * @const
 */
var Command = {
  EDIT: 'edit',
  COPY_URL: 'copy-url',
  SHOW_IN_FOLDER: 'show-in-folder',
  DELETE: 'delete',
  OPEN_NEW_TAB: 'open-new-tab',
  OPEN_NEW_WINDOW: 'open-new-window',
  OPEN_INCOGNITO: 'open-incognito',
  UNDO: 'undo',
  REDO: 'redo',
  // OPEN triggers when you double-click an item.
  OPEN: 'open',
  SELECT_ALL: 'select-all',
  DESELECT_ALL: 'deselect-all',
  COPY: 'copy',
  CUT: 'cut',
  PASTE: 'paste',
};

/**
 * @enum {number}
 * @const
 */
var MenuSource = {
  NONE: 0,
  LIST: 1,
  TREE: 2,
};

/**
 * Mirrors the C++ enum from IncognitoModePrefs.
 * @enum {number}
 * @const
 */
var IncognitoAvailability = {
  ENABLED: 0,
  DISABLED: 1,
  FORCED: 2,
};

/** @const */
var LOCAL_STORAGE_CLOSED_FOLDERS_KEY = 'closedState';

/** @const */
var LOCAL_STORAGE_TREE_WIDTH_KEY = 'treeWidth';

/** @const */
var ROOT_NODE_ID = '0';

/** @const */
var BOOKMARKS_BAR_ID = '1';

/** @const {number} */
var OPEN_CONFIRMATION_LIMIT = 15;
