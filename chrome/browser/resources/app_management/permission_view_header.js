// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-permission-view-header',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /** @type {App} */
    app: {
      type: Object,
    },
  },

  /**
   * @private
   */
  onClickBackButton_: function() {
    this.dispatch(app_management.actions.changePage(PageType.MAIN));
  },

  /**
   * @private
   */
  onClickUninstallButton_: function() {
    // TODO(rekanorman): Uncomment once backend uninstall implemented.
    // app_management.BrowserProxy.getInstance().handler.uninstall(this.app.id);
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    return app_management.util.getAppIcon(app);
  },
});
