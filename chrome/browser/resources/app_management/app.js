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
    this.listenerIds_ =
        [callbackRouter.onAppsAdded.addListener(this.onAppsAdded.bind(this))];
  },

  detached: function() {
    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach((id) => callbackRouter.removeListener(id));
  },

  onAppsAdded: function(apps) {
    const action = app_management.actions.addApps(apps);
    app_management.Store.getInstance().dispatch(action);
  },
});
