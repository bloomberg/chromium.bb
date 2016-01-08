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
    origin: String,

    /**
     * The amount of data stored for the origin.
     */
    storedData_: {
      type: String,
      observer: 'onStoredDataChanged_',
    },
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

    this.storedData_ = '1337 MB';  // TODO(finnur): Fetch actual data.
  },

  onStoredDataChanged_: function() {
    this.$.usage.hidden = false;
    this.$.storage.hidden = false;
  },

  onClearStorage_: function() {
    // TODO(finnur): Implement.
  },

  onClearAndReset_: function() {
    Array.prototype.forEach.call(
        this.root.querySelectorAll('site-details-permission'),
        function(element) { element.resetPermission(); });

    this.onClearStorage_();
  },
});
