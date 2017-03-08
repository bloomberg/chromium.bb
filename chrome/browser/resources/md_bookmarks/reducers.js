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
  /**
   * Root reducer for the Bookmarks page. This is called by the store in
   * response to an action, and the return value is used to update the UI.
   * @param {!BookmarksPageState} state
   * @param {Action} action
   * @return {!BookmarksPageState}
   */
  function reduceAction(state, action) {
    return {};
  }

  return {
    reduceAction: reduceAction,
  };
});
