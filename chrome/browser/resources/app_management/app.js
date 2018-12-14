// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-app',

  /** @override */
  attached: function() {
    app_management.BrowserProxy.getInstance().handler.getApps();
    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;

    app_management.Store.getInstance().init(
        app_management.util.createEmptyState());
    this.listenerIds_ = [
      callbackRouter.onAppsAdded.addListener(this.onAppsAdded.bind(this)),
      callbackRouter.onAppChanged.addListener(this.onAppChanged.bind(this)),
      callbackRouter.onAppRemoved.addListener(this.onAppRemoved.bind(this)),
    ];
  },

  detached: function() {
    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach((id) => callbackRouter.removeListener(id));
  },

  /**
   * @param {cr.ui.Action} action
   */
  dispatch: function(action) {
    app_management.Store.getInstance().dispatch(action);
  },

  /**
   * @param {Array<appManagement.mojom.App>} apps
   */
  onAppsAdded: function(apps) {
    const action = app_management.actions.addApps(apps);
    this.dispatch(action);
  },

  /**
   * @param {appManagement.mojom.App} app
   */
  onAppChanged: function(app) {
    const action = app_management.actions.changeApp(app);
    this.dispatch(action);
  },

  /**
   * @param {string} appId
   */
  onAppRemoved: function(appId) {
    const action = app_management.actions.removeApp(appId);
    this.dispatch(action);
  },
});
