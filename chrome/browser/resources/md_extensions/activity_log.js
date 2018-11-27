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
       * @private
       * @type {!chrome.activityLogPrivate.ActivityResultSet|undefined}
       */
      activityData_: Object,

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
        if (newPage.page === Page.ACTIVITY_LOG)
          this.getActivityLog_();
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
          (!this.activityData_ || this.activityData_.activities.length === 0);
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
          !!this.activityData_ && this.activityData_.activities.length > 0;
    },

    /** @private */
    onCloseButtonTap_: function() {
      extensions.navigation.navigateTo(
          {page: Page.DETAILS, extensionId: this.extensionId});
    },

    /** @private */
    getActivityLog_: function() {
      this.pageState_ = ActivityLogPageState.LOADING;
      this.delegate.getExtensionActivityLog(this.extensionId).then(result => {
        this.pageState_ = ActivityLogPageState.LOADED;
        this.activityData_ = result;
        this.onDataFetched.resolve();
      });
    },
  });

  return {
    ActivityLog: ActivityLog,
    ActivityLogDelegate: ActivityLogDelegate,
  };
});
