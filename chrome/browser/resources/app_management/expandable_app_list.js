// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-expandable-app-list',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * @private {Page}
     */
    currentPage_: {
      type: Object,
      observer: 'onViewChanged_',
    },

    /**
     * List of apps displayed before expanding the app list.
     * @type {Array<App>}
     */
    displayedApps: Array,

    /**
     * Title of the expandable list.
     * @type {String}
     */
    listTitle: String,

    /**
     * List of apps displayed after expanding app list.
     * @type {Array<App>}
     */
    collapsedApps: {
      type: Array,
      observer: 'onAppsChanged_',
    },

    /**
     * @private {boolean}
     */
    listExpanded_: Boolean,
  },

  attached: function() {
    this.watch('currentPage_', state => state.currentPage);

    this.updateFromStore();
  },

  /**
   * @private
   */
  onAppsChanged_: function() {
    this.$['expander-row'].hidden = this.collapsedApps.length === 0;
  },

  /**
   * Collapse the list when changing a page if it is open so that list is always
   * collapsed when entering the page.
   * @private
   */
  onViewChanged_: function() {
    this.$['app-list-title'].hidden = !this.listTitle;
    this.listExpanded_ = false;
  },

  /**
   * @private
   */
  toggleListExpanded_: function() {
    this.listExpanded_ = !this.listExpanded_;
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
   * @param {number} numApps
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  moreAppsString_: function(numApps, listExpanded) {
    return listExpanded ? loadTimeData.getString('lessApps') :
                          loadTimeData.getStringF('moreApps', numApps);
  },

  notificationsViewSelected_: function() {
    return this.currentPage_.pageType === PageType.NOTIFICATIONS;
  },
});
