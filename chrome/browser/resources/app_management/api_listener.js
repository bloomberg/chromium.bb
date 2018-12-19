// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('app_management.ApiListener', function() {
  let initialized = false;

  function init() {
    assert(!initialized);
    app_management.BrowserProxy.getInstance().handler.getApps();
    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;

    app_management.Store.getInstance().init(
        app_management.util.createEmptyState());

    callbackRouter.onAppsAdded.addListener(onAppsAdded.bind(this));
    callbackRouter.onAppChanged.addListener(onAppChanged.bind(this));
    callbackRouter.onAppRemoved.addListener(onAppRemoved.bind(this));
    initialized = true;
  }

  /**
   * @param {cr.ui.Action} action
   */
  function dispatch(action) {
    app_management.Store.getInstance().dispatch(action);
  }

  /**
   * @param {Array<appManagement.mojom.App>} apps
   */
  function onAppsAdded(apps) {
    dispatch(app_management.actions.addApps(apps));
  }

  /**
   * @param {appManagement.mojom.App} app
   */
  function onAppChanged(app) {
    dispatch(app_management.actions.changeApp(app));
  }

  /**
   * @param {string} appId
   */
  function onAppRemoved(appId) {
    dispatch(app_management.actions.removeApp(appId));
  }

  init();
});
