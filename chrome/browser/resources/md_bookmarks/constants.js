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

/** @const */
var LOCAL_STORAGE_CLOSED_FOLDERS_KEY = 'bookmarkManagerClosedState';

/** @const */
var ROOT_NODE_ID = '0';
