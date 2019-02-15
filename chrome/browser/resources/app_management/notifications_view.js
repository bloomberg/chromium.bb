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
     * @private {AppMap}
     */
    apps_: {
      type: Object,
      observer: 'onAppsChanged_',
    },

    /**
     * List of apps with notification permission
     * displayed before expanding the app list.
     * @private {!Array<App>}
     */
    displayedApps_: {
      type: Array,
      value: () => [],
    },

    /**
     * List of apps without notification permission
     * displayed after expanding app list.
     * @private {!Array<App>}
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
  },

  attached: function() {
    this.watch('apps_', state => state.apps);
    this.updateFromStore();

    this.onViewLoaded_();
  },

  /**
   * Creates arrays of displayed and collapsed apps based on the sets of apps
   * with notifications allowed and blocked in the Store. The orders of apps
   * in these arrays should then remain fixed while this view is showing.
   *
   * If all the apps have / don't have notification permission, display the
   * whole list, else display those with notification permission before
   * expanding.
   * @private
   */
  onViewLoaded_: function() {
    const state = this.getState();
    this.displayedApps_ =
        Array.from(state.notifications.allowedIds, id => state.apps[id]);
    this.collapsedApps_ =
        Array.from(state.notifications.blockedIds, id => state.apps[id]);

    if (this.displayedApps_.length === 0) {
      this.displayedApps_ = this.collapsedApps_;
      this.collapsedApps_ = [];
    }
  },

  /**
   * Updates the lists of displayed and collapsed apps when any changes occur
   * to the apps in the Store, maintaining the original order of apps in the
   * lists. New lists are created so that Polymer bindings will re-evaluate.
   * @private
   */
  onAppsChanged_() {
    const unhandledAppIds = new Set(Object.keys(this.apps_));
    this.displayedApps_ =
        this.updateAppList_(this.displayedApps_, unhandledAppIds);
    this.collapsedApps_ =
        this.updateAppList_(this.collapsedApps_, unhandledAppIds);

    // If any new apps have been added, append them to the appropriate list.
    for (const appId of unhandledAppIds) {
      const app = this.apps_[appId];
      const allowed = app_management.util.notificationsAllowed(app);

      if (allowed === OptionalBool.kUnknown) {
        continue;
      }

      if (allowed === OptionalBool.kTrue) {
        this.displayedApps_.push(app);
      } else {
        this.collapsedApps_.push(app);
      }
    }
  },

  /**
   * Creates a new list of apps with the same order as the original appList,
   * but using the updated apps from this.apps_. As each app is added to the
   * new list, it is also removed from the unhandledAppIds set.
   * @param {!Array<App>} appList
   * @param {!Set<string>} unhandledAppIds
   * @return {!Array<App>}
   * @private
   */
  updateAppList_(appList, unhandledAppIds) {
    const newApps = [];
    for (const app of appList) {
      if (unhandledAppIds.has(app.id)) {
        newApps.push(this.apps_[app.id]);
        unhandledAppIds.delete(app.id);
      }
    }
    return newApps;
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
   * Returns a boolean representation of the permission value, which used to
   * determine the position of the permission toggle.
   * @param {App} app
   * @return {boolean}
   * @private
   */
  getNotificationValueBool_: function(app) {
    return app_management.util.getPermissionValueBool(
        app, this.notificationsPermissionType(app));
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  notificationsPermissionType_: function(app) {
    return assert(app_management.util.notificationsPermissionType(app));
  },
});
