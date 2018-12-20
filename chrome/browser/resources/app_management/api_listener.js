// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('app_management.ApiListener', function() {
  let initialized = false;

  let initialListenerId;

  function init() {
    assert(!initialized);

    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;

    initialListenerId =
        callbackRouter.onAppsAdded.addListener(initialOnAppsAdded);

    app_management.BrowserProxy.getInstance().handler.getApps();

    initialized = true;
  }

  /**
   * @param {!Array<App>} apps
   */
  function initialOnAppsAdded(apps) {
    const initialState = app_management.util.createInitialState(apps);
    app_management.Store.getInstance().init(initialState);

    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;

    callbackRouter.onAppsAdded.addListener(onAppsAdded);
    callbackRouter.onAppChanged.addListener(onAppChanged);
    callbackRouter.onAppRemoved.addListener(onAppRemoved);

    callbackRouter.removeListener(initialListenerId);
  }

  /**
   * @param {cr.ui.Action} action
   */
  function dispatch(action) {
    app_management.Store.getInstance().dispatch(action);
  }

  /**
   * @param {Array<App>} apps
   */
  function onAppsAdded(apps) {
    dispatch(app_management.actions.addApps(apps));
  }

  /**
   * @param {App} app
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
