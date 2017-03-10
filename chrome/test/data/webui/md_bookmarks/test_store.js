// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('bookmarks', function() {
  var TestStore = function(data) {
    this.data = Object.assign(bookmarks.util.createEmptyState(), data);
    this.lastAction_ = null;
    this.observers_ = [];
  };

  TestStore.prototype = {
    addObserver: function(client) {
      this.observers_.push(client);
    },

    removeObserver: function(client) {},

    isInitialized: function() {
      return true;
    },

    handleAction: function(action) {
      this.lastAction_ = action;
    },

    get lastAction() {
      return this.lastAction_;
    },

    notifyObservers: function() {
      // TODO(tsergeant): Revisit how state modifications work in UI tests.
      // We don't want tests to worry about modifying the whole state tree.
      // Instead, we could perform a deep clone in here to ensure that every
      // StoreClient is updated.
      this.observers_.forEach((client) => client.onStateChanged(this.data));
    },
  };

  return {
    TestStore: TestStore,
  };
});
