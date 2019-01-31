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
     * List of apps with notification permission
     * displayed before expanding the app list.
     * @private {Array<App>}
     */
    displayedApps_: Array,

    /**
     * List of apps without notification permission
     * displayed after expanding app list.
     * @private {Array<App>}
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
     * A boolean to identify whether current page is notifications view so that
     * apps' list only get updated when page refreshed.
     * @private {boolean}
     */
    notificationsViewSelected_: {
      type: Boolean,
      observer: 'onViewChanged_',
    },
  },

  attached: function() {
    this.watch('notificationsViewSelected_', (state) => {
      return app_management.util.isNotificationsViewSelected(state);
    });
    this.updateFromStore();
  },

  /**
   * If all the apps have / don't have notification permission, display the
   * whole list, else display those with notification permission before
   * expanding.
   * @private
   */
  onViewChanged_: function() {
    [this.displayedApps_, this.collapsedApps_] =
        app_management.util.splitByNotificationPermission();
    if (this.displayedApps_.length === 0) {
      this.displayedApps_ = this.collapsedApps_;
      this.collapsedApps_ = [];
    }
    this.$['expander-row'].hidden = this.collapsedApps_.length === 0;
  },

  /**
   * @param {boolean} listExpanded
   * @param {Array<App>} collapsedApps
   * @return {string}
   * @private
   */
  moreAppsString_: function(listExpanded, collapsedApps) {
    return listExpanded ?
        loadTimeData.getString('lessApps') :
        loadTimeData.getStringF('moreApps', collapsedApps.length);
  },

  /**
   * @private
   */
  toggleListExpanded_: function() {
    this.listExpanded_ = !this.listExpanded_;
  },

  /** @private */
  onClickBackButton_: function() {
    this.listExpanded_ = false;
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

  /**
   * TODO(rekanorman) Make this function work for other apps.
   * Returns a boolean representation of the permission value, which used to
   * determine the position of the permission toggle.
   * @param {App} app
   * @return {boolean}
   * @private
   */
  getNotificationValueBool_: function(app) {
    return app_management.util.getPermissionValueBool(
        app, 'CONTENT_SETTINGS_TYPE_NOTIFICATIONS');
  },
});
