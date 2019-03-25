// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Subpages/views for the activity log. HISTORY shows extension activities
 * fetched from the activity log database with some fields such as args
 * omitted. STREAM displays extension activities in a more verbose format in
 * real time. NONE is used when user is away from the page.
 * @enum {number}
 */
const ActivityLogSubpage = {
  NONE: -1,
  HISTORY: 0,
  STREAM: 1
};

cr.define('extensions', function() {
  'use strict';

  const ActivityLog = Polymer({
    is: 'extensions-activity-log',

    behaviors: [
      CrContainerShadowBehavior,
      I18nBehavior,
      extensions.ItemBehavior,
    ],

    properties: {
      /**
       * The underlying ExtensionInfo for the details being displayed.
       * @type {!chrome.developerPrivate.ExtensionInfo}
       */
      extensionInfo: Object,

      /** @type {!extensions.ActivityLogDelegate} */
      delegate: Object,

      /** @private {!ActivityLogSubpage} */
      selectedSubpage_: {
        type: Number,
        value: ActivityLogSubpage.NONE,
      },
    },

    listeners: {
      'view-enter-start': 'onViewEnterStart_',
      'view-exit-finish': 'onViewExitFinish_',
    },

    /**
     * Focuses the back button when page is loaded and set the activie view to
     * be HISTORY when we navigate to the page.
     * @private
     */
    onViewEnterStart_: function() {
      this.selectedSubpage_ = ActivityLogSubpage.HISTORY;
      Polymer.RenderStatus.afterNextRender(
          this, () => cr.ui.focusWithoutInk(this.$.closeButton));
    },

    /**
     * Set |selectedSubpage_| to NONE to remove the active view from the DOM.
     * @private
     */
    onViewExitFinish_: function() {
      this.selectedSubpage_ = ActivityLogSubpage.NONE;
    },

    /**
     * @private
     * @return {boolean}
     */
    isHistoryTabSelected_: function() {
      return this.selectedSubpage_ === ActivityLogSubpage.HISTORY;
    },

    /**
     * @private
     * @return {boolean}
     */
    isStreamTabSelected_: function() {
      return this.selectedSubpage_ === ActivityLogSubpage.STREAM;
    },

    /** @private */
    onCloseButtonTap_: function() {
      extensions.navigation.navigateTo(
          {page: Page.DETAILS, extensionId: this.extensionInfo.id});
    },
  });

  return {
    ActivityLog: ActivityLog,
  };
});
