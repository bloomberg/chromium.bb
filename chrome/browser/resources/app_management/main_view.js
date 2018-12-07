// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-main-view',

  properties: {
    apps_: {
      type: Array,
    }
  },

  attached: function() {
    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_ =
        [callbackRouter.onAppsAdded.addListener((ids) => this.apps_ = ids)];
  },

  detached: function() {
    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach((id) => callbackRouter.removeListener(id));
  },

  iconUrlFromId_: function(id) {
    return `chrome://extension-icon/${id}/128/1`;
  }
});
