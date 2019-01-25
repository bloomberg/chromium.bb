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

  async function addApps(apps) {
    for (const app of apps) {
      callbackRouterProxy.onAppAdded(app);
    }
    await callbackRouterProxy.flushForTesting();
  }

  test('simple app addition', async function() {
    // Ensure there is no apps initially
    expectEquals(
        0, mainView.root.querySelectorAll('app-management-app-item').length);

    let apps = createTestApps(1);
    await addApps(apps);
    let appItems = mainView.root.querySelectorAll('app-management-app-item');
    expectEquals(1, appItems.length);

    expectEquals(appItems[0].app.id, apps[0].id);
  });

  test('more apps bar visibility', async function() {
    // The more apps bar shouldn't appear when there are 4 apps.
    await addApps(createTestApps(4));
    expectEquals(
        4, mainView.root.querySelectorAll('app-management-app-item').length);
    expectTrue(mainView.$['expander-row'].hidden);

    // The more apps bar appears when there are 5 apps.
    await addApps(createTestApps(1));
    expectEquals(
        5, mainView.root.querySelectorAll('app-management-app-item').length);
    expectFalse(mainView.$['expander-row'].hidden);
  });

  test('notifications sublabel collapsibility', async function() {
    // The three spans contains collapsible attribute.
    await addApps(createTestApps(4));
    const pieces = await mainView.getNotificationSublabelPieces_();
    expectTrue(pieces.filter(p => p.arg === '$1')[0].collapsible);
    expectTrue(pieces.filter(p => p.arg === '$2')[0].collapsible);
    expectTrue(pieces.filter(p => p.arg === '$3')[0].collapsible);

    // Checking ",and other x apps" is non-collapsible
    await addApps(createTestApps(6));
    const pieces2 = await mainView.getNotificationSublabelPieces_();
    expectFalse(pieces2.filter(p => p.arg === '$4')[0].collapsible);
  });
});
