// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<app-management-main-view>', function() {
  let mainView;
  let fakeHandler;
  let callbackRouterProxy;
  let appIdCounter;

  setup(function() {
    appIdCounter = 0;

    mainView = document.createElement('app-management-main-view');
    PolymerTest.clearBody();

    let browserProxy = app_management.BrowserProxy.getInstance();
    callbackRouterProxy = browserProxy.callbackRouter.createProxy();

    fakeHandler = new app_management.FakePageHandler(callbackRouterProxy);
    browserProxy.handler = fakeHandler;

    app_management.Store.instance_ = new app_management.Store();

    app_management.Store.getInstance().init(
        app_management.util.createEmptyState());

    document.body.appendChild(mainView);
  });

  /**
   * @param {number} numApps
   * @return {!Array<appManagement.mojom.App>} apps
   */
  function createTestApps(numApps) {
    let apps = [];
    for (let i = 0; i < numApps; i++) {
      apps.push(
          app_management.FakePageHandler.createApp('TestApp' + appIdCounter++));
    }
    return apps;
  }

  function addApps(apps) {
    for (const app of apps) {
      callbackRouterProxy.onAppAdded(app);
    }
  }

  test('simple app addition', async function() {
    let apps = createTestApps(1);
    addApps(apps);
    await callbackRouterProxy.flushForTesting();
    let appItems = mainView.root.querySelectorAll('app-management-item');
    expectEquals(1, appItems.length);

    expectEquals(appItems[0].app.id, apps[0].id);
  });

  test('more apps bar visibility', async function() {
    // The more apps bar shouldn't appear when there are no apps.
    expectEquals(
        0, mainView.root.querySelectorAll('app-management-item').length);
    expectTrue(mainView.$['expander-row'].hidden);

    // The more apps bar shouldn't appear when there are 4 apps.
    addApps(createTestApps(4));
    await callbackRouterProxy.flushForTesting();
    expectEquals(
        4, mainView.root.querySelectorAll('app-management-item').length);
    expectTrue(mainView.$['expander-row'].hidden);

    // The more apps bar appears when there are 5 apps.
    addApps(createTestApps(1));
    await callbackRouterProxy.flushForTesting();
    expectEquals(
        5, mainView.root.querySelectorAll('app-management-item').length);
    expectFalse(mainView.$['expander-row'].hidden);
  });
});
