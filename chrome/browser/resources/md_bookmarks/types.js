// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for MD Bookmarks.
 */

/** @typedef {!Object} */
var BookmarksPageState;

/** @typedef {{name: string}} */
var Action;

/** @interface */
function StoreObserver(){};

/** @param {!BookmarksPageState} newState */
StoreObserver.prototype.onStateChanged = function(newState) {};
