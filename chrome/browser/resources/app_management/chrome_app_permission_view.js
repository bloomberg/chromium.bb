// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-chrome-app-permission-view',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: Object,
  },

  attached: function() {
    this.watch('app_', function(state) {
      const selectedAppId = state.currentPage.selectedAppId;
      if (selectedAppId) {
        return state.apps[selectedAppId];
      }
    });

    this.updateFromStore();
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
