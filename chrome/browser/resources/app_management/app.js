// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-app',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * @private {Page}
     */
    currentPage_: {
      type: Object,
    },
  },

  /**
   * @override
   */
  attached: function() {
    this.watch('currentPage_', state => state.currentPage);
    this.updateFromStore();
  },

  /**
   * @param {Page} currentPage
   * @private
   */
  selectedRouteId_: function(currentPage) {
    switch (currentPage.pageType) {
      case (PageType.MAIN):
        return 'main-view';

      case (PageType.NOTIFICATIONS):
        return 'notifications-view';

      case (PageType.DETAIL):
        const state = app_management.Store.getInstance().data;
        const selectedAppType =
            state.apps[state.currentPage.selectedAppId].type;
        switch (selectedAppType) {
          case (AppType.kWeb):
            return 'pwa-permission-view';
          case (AppType.kExtension):
            return 'chrome-app-permission-view';
          default:
            assertNotReached();
        }

      default:
        assertNotReached();
    }
  },
});
