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
     * @private {boolean}
     */
    mainViewSelected_: Boolean,

    /**
     * @private {boolean}
     */
    notificationsViewSelected_: Boolean,

    /**
     * @private {boolean}
     */
    pwaPermissionViewSelected_: Boolean,

    /**
     * @private {boolean}
     */
    chromeAppPermissionViewSelected_: Boolean,
  },

  /** @override */
  attached: function() {
    // TODO(ceciliani) Generalize page selection in a nicer way.
    this.watch('mainViewSelected_', (state) => {
      return state.currentPage.pageType == PageType.MAIN;
    });

    this.watch('pwaPermissionViewSelected_', (state) => {
      // TODO(rekanorman): Remove AppType.kExtension case once PWA's are sent
      // thorough with the correct app type.
      return this.appTypeSelected(state, AppType.kWeb) ||
          this.appTypeSelected(state, AppType.kExtension);
    });

    this.watch('chromeAppPermissionViewSelected_', (state) => {
      return this.appTypeSelected(state, AppType.kExtension);
    });

    this.watch('notificationsViewSelected_', (state) => {
      return state.currentPage.pageType === PageType.NOTIFICATIONS;
    });

    this.updateFromStore();
  },

  /**
   * Returns true if the current page is the detail page of an app of the
   * given type.
   * @param {AppManagementPageState} state
   * @param {AppType} type
   * @return {boolean}
   */
  appTypeSelected: function(state, type) {
    if (!state.currentPage.selectedAppId) {
      return false;
    }

    const selectedApp = state.apps[state.currentPage.selectedAppId];
    return state.currentPage.pageType == PageType.DETAIL &&
        selectedApp.type == type;
  },
});
