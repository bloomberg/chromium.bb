// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<app-management-router>', function() {
  let store;
  let router;
  let fakeHandler;

  async function navigateTo(route) {
    window.history.replaceState({}, '', route);
    window.dispatchEvent(new CustomEvent('location-changed'));
    await PolymerTest.flushTasks();
  }

  function getCurrentUrlSuffix() {
    return window.location.href.slice(window.location.origin.length);
  }

  setup(async function() {
    fakeHandler = setupFakeHandler();
    store = new app_management.TestStore();
    await fakeHandler.addApp('1');
    store.replaceSingleton();
    router = document.createElement('app-management-router');
    replaceBody(router);
  });

  test('search updates from route', async function() {
    await navigateTo('/?q=beep');
    const expected = app_management.actions.setSearchTerm('beep');
    assertDeepEquals(expected, store.lastAction);
  });

  test('selected app updates from route', async function() {
    await navigateTo('/detail?id=1');
    const expected = app_management.actions.changePage(PageType.DETAIL, '1');

    assertDeepEquals(expected, store.lastAction);
  });

  test('notifications view appears from route', async function() {
    await navigateTo('/notifications');
    const expected = app_management.actions.changePage(PageType.NOTIFICATIONS);
    assertDeepEquals(expected, store.lastAction);
  });

  test('route updates from state change', async function() {
    // The application needs an initial url to start with.
    await navigateTo('/');

    store.data.currentPage = {
      pageType: PageType.DETAIL,
      selectedAppId: '1',
    };
    store.notifyObservers();

    await PolymerTest.flushTasks();
    assertEquals('/detail?id=1', getCurrentUrlSuffix());

    // Returning main page clears the route.
    store.data.currentPage = {
      pageType: PageType.MAIN,
      selectedAppId: null,
    };
    store.notifyObservers();
    await PolymerTest.flushTasks();
    assertEquals('/', getCurrentUrlSuffix());

    store.data.currentPage = {
      pageType: PageType.NOTIFICATIONS,
      selectedAppId: null,
    };
    store.notifyObservers();
    await PolymerTest.flushTasks();
    assertEquals('/notifications', getCurrentUrlSuffix());
  });

  test('route updates from search', async function() {
    await navigateTo('/');
    store.data.search = {term: 'bloop'};
    store.notifyObservers();
    await PolymerTest.flushTasks();

    assertEquals('/?q=bloop', getCurrentUrlSuffix());
  });
});
