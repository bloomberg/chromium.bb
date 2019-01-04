// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('extensions');

/**
 * The different states the activity log page can be in. Initial state is
 * LOADING because we call the activity log API whenever a user navigates to the
 * page. LOADED is the state where the API call has returned a successful
 * result.
 * @enum {string}
 */
const ActivityLogPageState = {
  LOADING: 'loading',
  LOADED: 'loaded'
};

cr.define('extensions', function() {
  'use strict';

  /** @interface */
  class ActivityLogDelegate {
    /**
     * @param {string} extensionId
     * @return {!Promise<!chrome.activityLogPrivate.ActivityResultSet>}
     */
    getExtensionActivityLog(extensionId) {}

    /**
     * @param {string} extensionId
     * @param {string} searchTerm
     * @return {!Promise<!chrome.activityLogPrivate.ActivityResultSet>}
     */
    getFilteredExtensionActivityLog(extensionId, searchTerm) {}

    /**
     * @param {string} extensionId
     * @return {!Promise<void>}
     */
    deleteActivitiesFromExtension(extensionId) {}
  }

  /**
   * Group activity log entries by the API call and merge their counts.
   * We currently assume that every API call matches to one activity type.
   * @param {!Array<!chrome.activityLogPrivate.ExtensionActivity>}
   *     activityData
   * @return {!Map<string, !extensions.ApiGroup>}
   */
  function groupActivitiesByApiCall(activityData) {
    const activitiesByApiCall = new Map();

    for (const activity of activityData) {
      const apiCall = activity.apiCall;
      const count = activity.count;
      const pageUrl = activity.pageUrl;

      if (!activitiesByApiCall.has(apiCall)) {
        const apiGroup = {
          apiCall,
          count,
          activityType: activity.activityType,
          countsByUrl: pageUrl ? new Map([[pageUrl, count]]) : new Map()
        };
        activitiesByApiCall.set(apiCall, apiGroup);
      } else {
        const apiGroup = activitiesByApiCall.get(apiCall);
        apiGroup.count += count;

        if (pageUrl) {
          const currentCount = apiGroup.countsByUrl.get(pageUrl) || 0;
          apiGroup.countsByUrl.set(pageUrl, currentCount + count);
        }
      }
    }

    return activitiesByApiCall;
  }

  /**
   * Sort activities by the total count for each API call. Resolve ties by the
   * alphabetical order of the API call name.
   * @param {!Map<string, !extensions.ApiGroup>} activitiesByApiCall
   * @return {!Array<!extensions.ApiGroup>}
   */
  function sortActivitiesByCallCount(activitiesByApiCall) {
    return Array.from(activitiesByApiCall.values()).sort(function(a, b) {
      if (a.count != b.count) {
        return b.count - a.count;
      }
      if (a.apiCall < b.apiCall) {
        return -1;
      }
      if (a.apiCall > b.apiCall) {
        return 1;
      }
      return 0;
    });
  }

  const ActivityLog = Polymer({
    is: 'extensions-activity-log',

    behaviors: [
      CrContainerShadowBehavior,
    ],

    properties: {
      /** @type {!string} */
      extensionId: String,

      /** @type {!extensions.ActivityLogDelegate} */
      delegate: Object,

      /**
       * An array representing the activity log. Stores activities grouped by
       * API call sorted in descending order of the call count.
       * @private
       * @type {!Array<!extensions.ApiGroup>}
       */
      activityData_: {
        type: Array,
        value: () => [],
      },

      /**
       * @private
       * @type {ActivityLogPageState}
       */
      pageState_: {
        type: String,
        value: ActivityLogPageState.LOADING,
      },

      /**
       * A promise resolver for any external files waiting for the
       * GetExtensionActivity API call to finish.
       * Currently only used for extension_settings_browsertest.cc
       * @type {!PromiseResolver}
       */
      onDataFetched: {type: Object, value: new PromiseResolver()},

      /** @private */
      lastSearch_: String,
    },

    /** @private {?number} */
    navigationListener_: null,

    /** @override */
    attached: function() {
      // Fetch the activity log for the extension when this page is attached.
      // This is necesary as the listener below is not fired if the user
      // navigates directly to the activity log page using url.
      this.getActivityLog_();

      // Add a listener here so we fetch the activity log whenever a user
      // navigates to the activity log from another page. This is needed since
      // this component already exists in the background if a user navigates
      // away from this page so attached may not be called when a user navigates
      // back.
      this.navigationListener_ = extensions.navigation.addListener(newPage => {
        if (newPage.page === Page.ACTIVITY_LOG) {
          this.getActivityLog_();
        }
      });
    },

    /** @override */
    detached: function() {
      assert(extensions.navigation.removeListener(this.navigationListener_));
      this.navigationListener_ = null;
    },

    /**
     * @private
     * @return {boolean}
     */
    shouldShowEmptyActivityLogMessage_: function() {
      return this.pageState_ === ActivityLogPageState.LOADED &&
          this.activityData_.length === 0;
    },

    /**
     * @private
     * @return {boolean}
     */
    shouldShowLoadingMessage_: function() {
      return this.pageState_ === ActivityLogPageState.LOADING;
    },

    /**
     * @private
     * @return {boolean}
     */
    shouldShowActivities_: function() {
      return this.pageState_ === ActivityLogPageState.LOADED &&
          this.activityData_.length > 0;
    },

    /** @private */
    onClearButtonTap_: function() {
      this.delegate.deleteActivitiesFromExtension(this.extensionId).then(() => {
        this.processActivities_([]);
      });
    },

    /** @private */
    onCloseButtonTap_: function() {
      extensions.navigation.navigateTo(
          {page: Page.DETAILS, extensionId: this.extensionId});
    },

    /**
     * @private
     * @param {!Array<!chrome.activityLogPrivate.ExtensionActivity>}
     *     activityData
     */
    processActivities_: function(activityData) {
      this.pageState_ = ActivityLogPageState.LOADED;
      this.activityData_ =
          sortActivitiesByCallCount(groupActivitiesByApiCall(activityData));
      if (!this.onDataFetched.isFulfilled) {
        this.onDataFetched.resolve();
      }
    },

    /** @private */
    getActivityLog_: function() {
      this.pageState_ = ActivityLogPageState.LOADING;
      this.delegate.getExtensionActivityLog(this.extensionId).then(result => {
        this.processActivities_(result.activities);
      });
    },

    /**
     * @private
     * @param {string} searchTerm
     */
    getFilteredActivityLog_: function(searchTerm) {
      this.pageState_ = ActivityLogPageState.LOADING;
      this.delegate
          .getFilteredExtensionActivityLog(this.extensionId, searchTerm)
          .then(result => {
            this.processActivities_(result.activities);
          });
    },

    /** @private */
    onSearchChanged_: function(e) {
      // Remove all whitespaces from the search term, as API call names and
      // urls should not contain any whitespace. As of now, only single term
      // search queries are allowed.
      const searchTerm = e.detail.replace(/\s+/g, '');
      if (searchTerm === this.lastSearch_) {
        return;
      }

      this.lastSearch_ = searchTerm;
      if (searchTerm === '') {
        this.getActivityLog_();
      } else {
        this.getFilteredActivityLog_(searchTerm);
      }
    },
  });

  return {
    ActivityLog: ActivityLog,
    ActivityLogDelegate: ActivityLogDelegate,
  };
});
