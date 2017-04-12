// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-router>', function() {
  var store;
  var router;

  function navigateTo(route) {
    window.history.replaceState({}, '', route);
    window.dispatchEvent(new CustomEvent('location-changed'));
  }

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(createFolder('1', [createFolder('2', [])])),
      selectedFolder: '1',
      search: {
        term: '',
      },
    });
    bookmarks.Store.instance_ = store;

    router = document.createElement('bookmarks-router');
    replaceBody(router);
  });

  test('search updates from route', function() {
    navigateTo('/?q=bleep');
    assertEquals('start-search', store.lastAction.name);
    assertEquals('bleep', store.lastAction.term);
  });

  test('selected folder updates from route', function() {
    navigateTo('/?id=2');
    assertEquals('select-folder', store.lastAction.name);
    assertEquals('2', store.lastAction.id);
  });

  test('route updates from ID', function() {
    store.data.selectedFolder = '2';
    store.notifyObservers();

    return Promise.resolve().then(function() {
      assertEquals('chrome://bookmarks/?id=2', window.location.href);
    });
  });

  test('route updates from search', function() {
    store.data.search = {term: 'bloop'};
    store.notifyObservers();

    return Promise.resolve()
        .then(function() {
          assertEquals('chrome://bookmarks/?q=bloop', window.location.href);

          // Ensure that the route doesn't change when the search finishes.
          store.data.selectedFolder = null;
          store.notifyObservers();
        })
        .then(function() {
          assertEquals('chrome://bookmarks/?q=bloop', window.location.href);
        });
  });
});

suite('URL preload', function() {
  /**
   * Reset the page state with a <bookmarks-app> and a clean Store, with the
   * given |url| to trigger routing initialization code.
   */
  function setupWithUrl(url) {
    PolymerTest.clearBody();
    bookmarks.Store.instance_ = undefined;
    window.history.replaceState({}, '', url);

    chrome.bookmarks.getTree = function(callback) {
      console.log('getTree');
      console.log(window.location.href);
      callback([
        createFolder(
            '0',
            [
              createFolder(
                  '1',
                  [
                    createFolder('11', []),
                  ]),
              createFolder(
                  '2',
                  [
                    createItem('21'),
                  ]),
            ]),
      ]);
    };

    app = document.createElement('bookmarks-app');
    document.body.appendChild(app);
  }

  test('loading a search URL performs a search', function() {
    var lastQuery;
    chrome.bookmarks.search = function(query) {
      lastQuery = query;
      return ['11'];
    };

    setupWithUrl('/?q=testQuery');
    assertEquals('testQuery', lastQuery);
  });

  test('loading a folder URL selects that folder', function() {
    setupWithUrl('/?id=2');
    var state = bookmarks.Store.getInstance().data;
    assertEquals('2', state.selectedFolder);
    assertDeepEquals(['21'], bookmarks.util.getDisplayedList(state));
  });
});
