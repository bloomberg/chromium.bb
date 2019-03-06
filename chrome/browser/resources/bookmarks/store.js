// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A singleton datastore for the Bookmarks page. Page state is
 * publicly readable, but can only be modified by dispatching an Action to
 * the store.
 */

cr.define('bookmarks', function() {
  /** @extends {cr.ui.Store<BookmarksPageState>} */
  class Store extends cr.ui.Store {
    constructor() {
      super(bookmarks.util.createEmptyState(), bookmarks.reduceAction);
    }
  }

  cr.addSingletonGetter(Store);

  return {
    Store: Store,
  };
});
