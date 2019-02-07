// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

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

      /** @private */
      lastSearch_: {
        type: String,
        value: '',
      },

      /** @private */
      showHistory_: {
        type: Boolean,
        value: true,
      },
    },

    listeners: {
      'view-enter-start': 'onViewEnterStart_',
      'view-exit-finish': 'onViewExitFinish_',
    },

    /**
     * Focuses the back button when page is loaded.
     * @private
     */
    onViewEnterStart_: function() {
      this.showHistory_ = true;
      Polymer.RenderStatus.afterNextRender(
          this, () => cr.ui.focusWithoutInk(this.$.closeButton));
    },

    /**
     * Set |showHistory_| to false to remove activity-log-history from the DOM.
     * @private
     */
    onViewExitFinish_: function() {
      this.showHistory_ = false;
    },

    /** @private */
    onClearButtonTap_: function() {
      const activityLogHistory = this.$$('activity-log-history');
      activityLogHistory.clearActivities();
    },

    /** @private */
    onCloseButtonTap_: function() {
      extensions.navigation.navigateTo(
          {page: Page.DETAILS, extensionId: this.extensionId});
    },

    /**
     * @private
     * @param {!CustomEvent<string>} e
     */
    onSearchChanged_: function(e) {
      // Remove all whitespaces from the search term, as API call names and
      // urls should not contain any whitespace. As of now, only single term
      // search queries are allowed.
      const searchTerm = e.detail.replace(/\s+/g, '');
      if (searchTerm === this.lastSearch_) {
        return;
      }

      this.lastSearch_ = searchTerm;
    },
  });

  return {
    ActivityLog: ActivityLog,
  };
});
