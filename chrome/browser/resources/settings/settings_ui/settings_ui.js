// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ui' implements the UI for the Settings page.
 *
 * Example:
 *
 *    <settings-ui prefs="{{prefs}}"></settings-ui>
 */
Polymer({
  is: 'settings-ui',

  properties: {
    /**
     * Preferences state.
     */
    prefs: Object,

    /** @type {?settings.DirectionDelegate} */
    directionDelegate: {
      observer: 'directionDelegateChanged_',
      type: Object,
    },

    /** @private {boolean} */
    toolbarSpinnerActive_: {
      type: Boolean,
      value: false,
    },

    /**
     * Dictionary defining page visibility.
     * @private {!GuestModePageVisibility}
     */
    pageVisibility_: Object,

    /** @private */
    drawerOpened_: Boolean,
  },

  /**
   * @override
   * @suppress {es5Strict} Object literals cannot contain duplicate keys in ES5
   *     strict mode.
   */
  ready: function() {
    this.$$('cr-toolbar').addEventListener('search-changed', function(e) {
      this.$$('settings-main').searchContents(e.detail);
    }.bind(this));

    window.addEventListener('popstate', function(e) {
      this.$$('app-drawer').close();
    }.bind(this));

    if (loadTimeData.getBoolean('isGuest')) {
      this.pageVisibility_ = {
        people: false,
        onStartup: false,
        reset: false,
<if expr="not chromeos">
        appearance: false,
        defaultBrowser: false,
        advancedSettings: false,
</if>
<if expr="chromeos">
        appearance: {
          setWallpaper: false,
          setTheme: false,
          homeButton: false,
          bookmarksBar: false,
          pageZoom: false,
        },
        advancedSettings: true,
        dateTime: {
          timeZoneSelector: false,
        },
        privacy: {
          searchPrediction: false,
          networkPrediction: false,
        },
        passwordsAndForms: false,
        downloads: {
          googleDrive: false,
        },
</if>
      };
    }
  },

  /**
   * @param {Event} event
   * @private
   */
  onIronActivate_: function(event) {
    if (event.detail.item.id != 'advancedPage')
      this.$$('app-drawer').close();
  },

  /** @private */
  onMenuButtonTap_: function() {
    this.$$('app-drawer').toggle();
  },

  /** @private */
  directionDelegateChanged_: function() {
    this.$$('app-drawer').align = this.directionDelegate.isRtl() ?
        'right' : 'left';
  },
});
