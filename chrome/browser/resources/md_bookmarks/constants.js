// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of valid drop locations relative to an element. These are
 * bit masks to allow combining multiple locations in a single value.
 * @enum {number}
 * @const
 */
const DropPosition = {
  NONE: 0,
  ABOVE: 1,
  ON: 2,
  BELOW: 4,
};

/**
 * Commands which can be handled by the CommandManager. This enum is also used
 * for metrics and should be kept in sync with BookmarkManagerCommand in
 * enums.xml.
 * @enum {number}
 * @const
 */
const Command = {
  EDIT: 0,
  COPY_URL: 1,
  SHOW_IN_FOLDER: 2,
  DELETE: 3,
  OPEN_NEW_TAB: 4,
  OPEN_NEW_WINDOW: 5,
  OPEN_INCOGNITO: 6,
  UNDO: 7,
  REDO: 8,
  // OPEN triggers when you double-click an item.
  OPEN: 9,
  SELECT_ALL: 10,
  DESELECT_ALL: 11,
  COPY: 12,
  CUT: 13,
  PASTE: 14,
  // Append new values to the end of the enum.
  MAX_VALUE: 15,
};

/**
 * @enum {number}
 * @const
 */
const MenuSource = {
  NONE: 0,
  LIST: 1,
  TREE: 2,
};

/**
 * Mirrors the C++ enum from IncognitoModePrefs.
 * @enum {number}
 * @const
 */
const IncognitoAvailability = {
  ENABLED: 0,
  DISABLED: 1,
  FORCED: 2,
};

/** @const */
const LOCAL_STORAGE_FOLDER_STATE_KEY = 'folderOpenState';

/** @const */
const LOCAL_STORAGE_TREE_WIDTH_KEY = 'treeWidth';

/** @const */
const ROOT_NODE_ID = '0';

/** @const */
const BOOKMARKS_BAR_ID = '1';

/** @const {number} */
const OPEN_CONFIRMATION_LIMIT = 15;

/**
 * Folders that are beneath this depth will be closed by default in the folder
 * tree (where the Bookmarks Bar folder is at depth 0).
 * @const {number}
 */
const FOLDER_OPEN_BY_DEFAULT_DEPTH = 1;
