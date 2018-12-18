// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-pwa-permission-view',
  properties: {
    /**
     * @type {appManagement.mojom.App}
     * @private
     */
    app_: {
      type: Object,
      value: function() {
        return app_management.FakePageHandler.createApp(
            'blpcfgokakmgnkcojhhkbfbldkacnbeo');
      },
    },

    /**
     * @private {boolean}
     */
    listExpanded_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @private
   */
  toggleListExpanded_: function() {
    this.listExpanded_ = !this.listExpanded_;
  },

  /**
   * @param {appManagement.mojom.App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    return `chrome://extension-icon/${app.id}/128/1`;
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
