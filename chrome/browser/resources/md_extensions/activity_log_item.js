// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * @typedef {{
   *   apiCall: string,
   *   count: number,
   *   activityType: !chrome.activityLogPrivate.ExtensionActivityFilter,
   *   countsByUrl: !Map<string, number>
   * }}
   */
  let ApiGroup;

  /**
   * @typedef {{
   *   page: string,
   *   count: number
   * }}
   */
  let PageUrlItem;

  const ActivityLogItem = Polymer({
    is: 'activity-log-item',

    properties: {
      /**
       * The underlying ApiGroup that provides data for the
       * ActivityLogItem displayed.
       * @type {!extensions.ApiGroup}
       */
      data: Object,
    },

    /**
     * Show page URLs if there is at least one non-empty page URL. We are
     * guaranteed to have at least one based on how the data in this component
     * is generated from activity_log.js.
     * @private
     * @return {boolean}
     */
    shouldShowPageUrls_: function() {
      return this.data.countsByUrl.size > 0;
    },

    /**
     * Show the API call count for a particular page URL if more than one page
     * URL is associated with this API call.
     * @private
     * @return {boolean}
     */
    shouldShowPageUrlCount_: function() {
      return this.data.countsByUrl.size > 1;
    },

    /**
     * Sort the page URLs by the number of times the API call was made for that
     * page URL. Resolve ties by the alphabetical order of the page URL.
     * @private
     * @return {!Array<PageUrlItem>}
     */
    getPageUrls_: function() {
      return Array.from(this.data.countsByUrl.entries())
          .map(e => ({page: e[0], count: e[1]}))
          .sort(function(a, b) {
            if (a.count != b.count) {
              return b.count - a.count;
            }
            return a.page < b.page ? -1 : (a.page > b.page ? 1 : 0);
          });
    },
  });

  return {
    ActivityLogItem: ActivityLogItem,
    ApiGroup: ApiGroup,
  };
});
