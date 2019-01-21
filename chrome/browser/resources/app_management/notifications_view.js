// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-notifications-view',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * List of all apps.
     * @private {Array<App>}
     */
    apps_: {
      type: Array,
      value: () => [],
    },

    /**
     * List of apps with notification permission
     * displayed before expanding the app list.
     * @private {Array<App>}
     */
    displayedApps_: {
      type: Array,
      value: function() {
        const /** @type {!Array<App>}*/ appList = [
          app_management.FakePageHandler.createApp(
              'blpcfgokakmgnkcojhhkbfbldkacnbeo'),
          app_management.FakePageHandler.createApp(
              'pjkljhegncpnkpknbcohdijeoejaedia'),
          app_management.FakePageHandler.createApp(
              'aapocclcgogkmnckokdopfmhonfmgoek'),
          app_management.FakePageHandler.createApp(
              'ahfgeienlihckogmohjhadlkjgocpleb'),
        ];
        return appList;
      },
    },

    /**
     * List of apps without notification permission
     * displayed after expanding app list.
     * @private {Array<App>}
     */
    collapsedApps_: {
      type: Array,
      value: function() {
        const /** @type {!Array<App>}*/ appList = [
          app_management.FakePageHandler.createApp(
              'aohghmighlieiainnegkcijnfilokake',
              {type: apps.mojom.AppType.kArc}),
        ];
        return appList;
      },
    },

    /**
     * @private {boolean}
     */
    listExpanded_: {
      type: Boolean,
      value: true,
    },
  },

  attached: function() {
    this.watch('apps_', (state) => {
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
    // TODO(ceciliani) Make view display apps with notification permission
    // first.
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
  },

  /** @private */
  onClickBackButton_: function() {
    if (!window.history.state) {
      this.dispatch(app_management.actions.changePage(PageType.MAIN));
    } else {
      window.history.back();
    }
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
