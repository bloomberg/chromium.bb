// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * 'settings-personalization-page' is the settings page containing
 * personalization settings.
 */
Polymer({
  is: 'settings-personalization-page',

  properties: {
    /**
     * Dictionary defining page visibility.
     * @type {!AppearancePageVisibility}
     */
    pageVisibility: Object,

    /** @private */
    isWallpaperPolicyControlled_: {type: Boolean, value: true},
  },

  /** @private {?settings.PersonalizationBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.PersonalizationBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.browserProxy_.isWallpaperSettingVisible().then(
        isWallpaperSettingVisible => {
            // TODO(hsuregan): Uncomment after forking new pageVisibility for
            // OS settings.
            // assert(this.pageVisibility);
            // this.pageVisibility.setWallpaper = isWallpaperSettingVisible;
        });
    this.browserProxy_.isWallpaperPolicyControlled().then(
        isPolicyControlled => {
          this.isWallpaperPolicyControlled_ = isPolicyControlled;
        });
  },

  /**
   * @private
   */
  openWallpaperManager_: function() {
    this.browserProxy_.openWallpaperManager();
  },
});
})();
