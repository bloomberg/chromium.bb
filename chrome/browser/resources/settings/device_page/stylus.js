// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-stylus' is the settings subpage with stylus-specific settings.
 */

/** @const */ var FIND_MORE_APPS_URL = 'https://play.google.com/store/apps/' +
    'collection/promotion_30023cb_stylus_apps';

Polymer({
  is: 'settings-stylus',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Note taking apps the user can pick between.
     * @type {Array<{name:string, value:string, preferred:boolean}>}
     * @private
     */
    appChoices_: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * True if the ARC container has not finished starting yet.
     * @private
     */
    waitingForAndroid_: {
      type: Boolean,
      value: false
    },
  },


  /** @private {?settings.DevicePageBrowserProxy} */
  browserProxy_: null,

  created: function() {
    this.browserProxy_ = settings.DevicePageBrowserProxyImpl.getInstance();
  },

  ready: function() {
    this.browserProxy_.setNoteTakingAppsUpdatedCallback(
        this.onNoteAppsUpdated_.bind(this));
    this.browserProxy_.requestNoteTakingApps();
  },

  /** @private */
  onSelectedAppChanged_: function() {
    this.browserProxy_.setPreferredNoteTakingApp(this.$.menu.value);
  },

  /**
   * @param {Array<settings.NoteAppInfo>} apps
   * @param {boolean} waitingForAndroid
   * @private
   */
  onNoteAppsUpdated_: function(apps, waitingForAndroid) {
    this.waitingForAndroid_ = waitingForAndroid;
    this.appChoices_ = apps;
  },

  /**
   * @param {Array<settings.NoteAppInfo>} apps
   * @param {boolean} waitingForAndroid
   * @private
   */
  showNoApps_: function(apps, waitingForAndroid) {
    return apps.length == 0 && !waitingForAndroid;
  },

  /**
   * @param {Array<settings.NoteAppInfo>} apps
   * @param {boolean} waitingForAndroid
   * @private
   */
  showApps_: function(apps, waitingForAndroid) {
    return apps.length > 0 && !waitingForAndroid;
  },

  /** @private */
  onFindAppsTap_: function() {
    this.browserProxy_.showPlayStore(FIND_MORE_APPS_URL);
  },
});
