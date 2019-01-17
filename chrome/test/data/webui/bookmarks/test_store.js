// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suiteSetup(function() {
  cr.define('bookmarks', function() {
    class TestStore extends cr.ui.TestStore {
      constructor(data) {
        super(
            data, bookmarks.Store, bookmarks.util.createEmptyState(),
            bookmarks.reduceAction);
      }
    }

    return {
      TestStore: TestStore,
    };
  });
});
