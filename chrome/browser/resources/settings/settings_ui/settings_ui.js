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

    /** @private */
    appealClosed_: {
      type: Boolean,
      value: function() {
        return !!(window.sessionStorage.appealClosed_ ||
                  window.localStorage.appealClosed_);
      },
    },

    /** @private */
    appealBugUrl_: {
      type: String,
      value: function() {
        var url =
            'https://bugs.chromium.org/p/chromium/issues/entry?' +
            'labels=Proj-MaterialDesign-WebUI,Pri-2,Type-Bug&' +
            'components=UI%3ESettings&description=';
        var description =
            'What steps will reproduce the problem?\n' +
            '(1) \n(2) \n(3) \n\nWhat is the expected result?\n\n\n' +
            'What happens instead?\n\n\nPlease provide any additional ' +
            'information below. Attach a screenshot if possible.\n\n';
        var version = navigator.userAgent.match(/Chrom(?:e|ium)\/([\d.]+)/);
        if (version)
          description += 'Version: ' + version[1];
        url += encodeURIComponent(description);
        return url;
      },
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

  listeners: {
    'sideNav.iron-activate': 'onIronActivate_',
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

  /** @private */
  onCloseAppealTap_: function() {
    window.sessionStorage.appealClosed_ = this.appealClosed_ = true;
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
