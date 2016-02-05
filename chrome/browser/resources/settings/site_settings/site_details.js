// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details' show the details (permissions and usage) for a given origin
 * under Site Settings.
 *
 * Example:
 *
 *      <site-details prefs="{{prefs}}" origin="{{origin}}">
 *      </site-details>
 *      ... other pages ...
 *
 * @group Chrome Settings Elements
 * @element site-details
 */
Polymer({
  is: 'site-details',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The origin that this widget is showing details for.
     */
    origin: {
      type: String,
      observer: 'onOriginChanged_',
    },

    /**
     * The amount of data stored for the origin.
     */
    storedData_: {
      type: String,
      value: '',
    },

    /**
     * The type of storage for the origin.
     */
    storageType_: Number,

    /**
     * The category the user selected to get to this page.
     */
    categorySelected: String,

    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },
  },

  listeners: {
    'usage-deleted': 'onUsageDeleted',
  },

  ready: function() {
    this.$.cookies.category = settings.ContentSettingsTypes.COOKIES;
    this.$.javascript.category = settings.ContentSettingsTypes.JAVASCRIPT;
    this.$.popups.category = settings.ContentSettingsTypes.POPUPS;
    this.$.geolocation.category = settings.ContentSettingsTypes.GEOLOCATION;
    this.$.notification.category = settings.ContentSettingsTypes.NOTIFICATIONS;
    this.$.fullscreen.category = settings.ContentSettingsTypes.FULLSCREEN;
    this.$.camera.category = settings.ContentSettingsTypes.CAMERA;
    this.$.mic.category = settings.ContentSettingsTypes.MIC;
  },

  /**
   * Handler for when the origin changes.
   */
  onOriginChanged_: function() {
    var url = /** @type {{hostname: string}} */(new URL(this.origin));
    this.$.usageApi.fetchUsageTotal(url.hostname);
  },

  /**
   * Clears all data stored for the current origin.
   */
  onClearStorage_: function() {
    this.$.usageApi.clearUsage(this.origin, this.storageType_);
  },

  /**
   * Called when usage has been deleted for an origin.
   */
  onUsageDeleted: function(event) {
    if (event.detail.origin == this.origin) {
      this.storedData_ = '';
      this.navigateBackIfNoData_();
    }
  },

  /**
   * Resets all permissions and clears all data stored for the current origin.
   */
  onClearAndReset_: function() {
    Array.prototype.forEach.call(
        this.root.querySelectorAll('site-details-permission'),
        function(element) { element.resetPermission(); });

    if (this.storedData_ != '')
      this.onClearStorage_();
    else
      this.navigateBackIfNoData_();
  },

  /**
   * Navigate back if the UI is empty (everything been cleared).
   */
  navigateBackIfNoData_: function() {
    if (this.storedData_ == '' && !this.permissionShowing_())
      this.fire('subpage-back');
  },

  /**
   * Returns true if one or more permission is showing.
   */
  permissionShowing_: function() {
    return Array.prototype.some.call(
        this.root.querySelectorAll('site-details-permission'),
        function(element) { return element.offsetHeight > 0; });
  },
});
