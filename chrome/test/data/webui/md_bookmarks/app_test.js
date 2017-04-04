// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-app>', function() {
  var app;
  var store;

  function resetStore() {
    store = new bookmarks.TestStore({});
    store.acceptInitOnce();
    bookmarks.Store.instance_ = store;

    chrome.bookmarks.getTree = function(fn) {
      fn([
        createFolder(
            '0',
            [
              createFolder(
                  '1',
                  [
                    createFolder('11', []),
                  ]),
            ]),
      ]);
    };
  }

  setup(function() {
    resetStore();

    app = document.createElement('bookmarks-app');
    replaceBody(app);
  });

  teardown(function() {
    window.localStorage.clear();
  });

  test('write and load closed folder state', function() {
    var closedFoldersList = ['1'];
    var closedFolders = new Set(closedFoldersList);
    store.data.closedFolders = closedFolders;
    store.notifyObservers();

    // Ensure closed folders are written to local storage.
    assertDeepEquals(
        JSON.stringify(Array.from(closedFolders)),
        window.localStorage[LOCAL_STORAGE_CLOSED_FOLDERS_KEY]);

    resetStore();
    app = document.createElement('bookmarks-app');
    replaceBody(app);

    // Ensure closed folders are read from local storage.
    assertDeepEquals(closedFoldersList, normalizeSet(store.data.closedFolders));
  });
});
