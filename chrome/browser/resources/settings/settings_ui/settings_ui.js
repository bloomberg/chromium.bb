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
cr.exportPath('settings');
assert(!settings.defaultResourceLoaded,
       'settings_ui.js run twice. You probably have an invalid import.');
/** Global defined when the main Settings script runs. */
settings.defaultResourceLoaded = true;

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
      value: new settings.DirectionDelegateImpl(),
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
  },

  /** @override */
  created: function() {
    settings.initializeRouteFromUrl();
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

    // Lazy-create the drawer the first time it is opened or swiped into view.
    var drawer = assert(this.$$('app-drawer'));
    listenOnce(drawer, 'track opened-changed', function() {
      this.$.drawerTemplate.if = true;
    }.bind(this));

    window.addEventListener('popstate', function(e) {
      drawer.close();
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

  /** @override */
  attached: function() {
    // Preload bold Roboto so it doesn't load and flicker the first time used.
    document.fonts.load('bold 12px Roboto');
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
