// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-app>', () => {
  let app;
  let fakeHandler;
  let store;

  /** @return {ExpandableAppList} */
  function getAppList() {
    return app.$$('app-management-main-view')
        .$$('app-management-expandable-app-list');
  }

  setup(async () => {
    fakeHandler = setupFakeHandler();
    store = replaceStore();
    app = document.createElement('app-management-app');
    replaceBody(app);
    await app.$$('app-management-dom-switch').firstRenderForTesting_.promise;
    await PolymerTest.flushTasks();
  });

  test('loads', async () => {
    // Check that the browser responds to the getApps() message.
    const {apps: initialApps} =
        await app_management.BrowserProxy.getInstance().handler.getApps();
  });

  test('Searching switches to search page', async () => {
    app.$$('cr-toolbar').fire('search-changed', 'SearchTest');
    assert(app.$$('app-management-search-view'));
  });

  test('App list renders on page change', async (done) => {
    const appList = getAppList();

    await fakeHandler.addApp();
    let numApps = 1;

    expectEquals(numApps, appList.numChildrenForTesting_);

    // Click app to go to detail page.
    appList.querySelector('app-management-app-item').click();
    await PolymerTest.flushTasks();

    await fakeHandler.addApp();
    numApps++;

    appList.addEventListener('num-children-for-testing_-changed', () => {
      expectEquals(numApps, appList.numChildrenForTesting_);
      done();
    });

    // Click back button to go to main page.
    app.$$('app-management-pwa-permission-view')
        .$$('app-management-permission-view-header')
        .$$('#backButton')
        .click();
    await PolymerTest.flushTasks();
  });
});
