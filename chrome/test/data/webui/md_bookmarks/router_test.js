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
      selectedId: '1',
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
    navigateTo('/?id=5');
    assertEquals('select-folder', store.lastAction.name);
    assertEquals('5', store.lastAction.id);
  });

  test('route updates from ID', function() {
    store.data.selectedFolder = '6';
    store.notifyObservers();

    return Promise.resolve().then(function() {
      assertEquals('chrome://bookmarks/?id=6', window.location.href);
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
  test('loading a search URL performs a search', function(done) {
    function verifySearch(query) {
      assertEquals('testQuery', query);
      done();
    }

    if (window.searchedQuery) {
      verifySearch(window.searchedQuery);
      return;
    }

    chrome.bookmarks.search = verifySearch;
  });
});
