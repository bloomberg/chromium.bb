// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-pwa-permission-view',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: Object,

    /**
     * @private {boolean}
     */
    listExpanded_: {
      type: Boolean,
      value: false,
    },
  },

  attached: function() {
    this.watch('app_', (state) => {
      const selectedAppId = state.currentPage.selectedAppId;
      if (selectedAppId) {
        return state.apps[selectedAppId];
      }
    });

    this.updateFromStore();
  },

  onClickSiteSettingsButton_: function() {
    app_management.BrowserProxy.getInstance().handler.openNativeSettings(
        this.app_.id);
  },

  /**
   * @private
   */
  toggleListExpanded_: function() {
    this.listExpanded_ = !this.listExpanded_;
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    return app_management.util.getAppIcon(app);
  },

  /**
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  getCollapsedIcon_: function(listExpanded) {
    return listExpanded ? 'cr:expand-less' : 'cr:expand-more';
  },
});
