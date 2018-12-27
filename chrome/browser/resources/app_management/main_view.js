// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-main-view',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * List of all apps.
     * @private {Array<appManagement.mojom.App>}
     */
    apps_: {
      type: Array,
      value: () => [],
      observer: 'onAppsChanged_',
    },

    /**
     * List of apps displayed before expanding the app list.
     * @private {Array<appManagement.mojom.App>}
     */
    displayedApps_: {
      type: Array,
      value: () => [],
    },

    /**
     * List of apps displayed after expanding app list.
     * @private {Array<appManagement.mojom.App>}
     */
    collapsedApps_: {
      type: Array,
      value: () => [],
    },

    /**
     * @private {boolean}
     */
    listExpanded_: {
      type: Boolean,
      value: false,
    },

    /**
     * List of apps with the notification permission.
     * @private {Array<appManagement.mojom.App>}
     */
     notificationApps_: {
      type: Array,
      value: function() {
        const apps = [];
        for (let i = 0; i < NUMBER_OF_APPS_DISPLAYED_DEFAULT; i++) {
          apps.push(
          app_management.FakePageHandler.createApp('Notified' + i));
        }
        return apps;
      },
    },
  },

  attached: function() {
    this.watch('apps_', function(state) {
      return Object.values(state.apps);
    });
    this.updateFromStore();
  },

  /**
   * @param {number} numApps
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  moreAppsString_: function(numApps, listExpanded) {
    return listExpanded ?
        loadTimeData.getString('lessApps') :
        loadTimeData.getStringF(
            'moreApps', numApps - NUMBER_OF_APPS_DISPLAYED_DEFAULT);
  },

  /**
   * @private
   */
  toggleListExpanded_: function() {
    this.listExpanded_ = !this.listExpanded_;
    this.onAppsChanged_();
  },

  /**
   * @private
   */
  onAppsChanged_: function() {
    this.$['expander-row'].hidden =
        this.apps_.length <= NUMBER_OF_APPS_DISPLAYED_DEFAULT;
    this.displayedApps_ = this.apps_.slice(0, NUMBER_OF_APPS_DISPLAYED_DEFAULT);
    this.collapsedApps_ =
        this.apps_.slice(NUMBER_OF_APPS_DISPLAYED_DEFAULT, this.apps_.length);
  },

  /**
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  getCollapsedIcon_: function(listExpanded) {
    return listExpanded ? 'cr:expand-less' : 'cr:expand-more';
  },

  /**
   * @param {Array<appManagement.mojom.App>} notificationApps
   * @return {string}
   * @private
   */
   getNotificationSublabel_: function(notificationApps){
    return loadTimeData.getStringF(
        'notificationSublabel', notificationApps.length);
   }
});
