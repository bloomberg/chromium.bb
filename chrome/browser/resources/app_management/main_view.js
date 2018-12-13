// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-main-view',

  properties: {
    /**
     * List of all apps.
     * @private {Array<string>}
     */
    apps_: {
      type: Array,
      value: () => [],
      observer: 'onAppsChanged_',

    },
    /**
     * List of apps displayed before expanding the app list.
     * @private {Array<string>}
     */
    displayedApps_: {
      type: Array,
      value: () => [],
    },

    /**
     * List of apps displayed after expanding app list.
     * @private {Array<string>}
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
    }
  },

  attached: function() {
    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_ =
        [callbackRouter.onAppsAdded.addListener((ids) => this.apps_ = ids)];
  },

  detached: function() {
    const callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach((id) => callbackRouter.removeListener(id));
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
    this.$['more-apps'].hidden =
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
});
