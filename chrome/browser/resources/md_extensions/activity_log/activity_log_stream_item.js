// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * @typedef {{
   *   name: string,
   *   timestamp: number,
   *   activityType: !chrome.activityLogPrivate.ExtensionActivityFilter,
   *   pageUrl: string,
   *   args: ?String
   * }}
   */
  let StreamItem;

  const ActivityLogStreamItem = Polymer({
    is: 'activity-log-stream-item',

    properties: {
      /**
       * The underlying ActivityGroup that provides data for the
       * ActivityLogItem displayed.
       * @type {!extensions.StreamItem}
       */
      data: Object,

      /** @private */
      isExpandable_: {
        type: Boolean,
        computed: 'computeIsExpandable_(data)',
      },

      /** @private */
      isExpanded_: {
        type: Boolean,
        value: false,
      },
    },

    /**
     * @private
     * @return {boolean}
     */
    computeIsExpandable_: function() {
      return this.hasPageUrl_() || this.hasArgs_();
    },

    /**
     * @private
     * @return {string}
     */
    getFormattedTime_: function() {
      // Format the activity's time to HH:MM:SS.mmm format. Use ToLocaleString
      // for HH:MM:SS and padLeft for milliseconds.
      const activityDate = new Date(this.data.timestamp);
      const timeString = activityDate.toLocaleTimeString(undefined, {
        hour12: false,
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
      });

      const ms = activityDate.getMilliseconds().toString().padStart(3, '0');
      return `${timeString}.${ms}`;
    },

    /**
     * @private
     * @return {boolean}
     */
    hasArgs_: function() {
      return !!this.data.args && this.data.args != 'null';
    },

    /**
     * @private
     * @return {boolean}
     */
    hasPageUrl_: function() {
      return !!this.data.pageUrl;
    },

    /** @private */
    onExpandClick_: function() {
      if (this.isExpandable_) {
        this.isExpanded_ = !this.isExpanded_;
      }
    },
  });

  return {
    ActivityLogStreamItem: ActivityLogStreamItem,
    StreamItem: StreamItem,
  };
});
