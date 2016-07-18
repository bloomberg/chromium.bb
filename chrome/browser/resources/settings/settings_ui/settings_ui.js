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
     * @type {?CrSettingsPrefsElement}
     */
    prefs: Object,

    /** @type {?settings.DirectionDelegate} */
    directionDelegate: {
      observer: 'directionDelegateChanged_',
      type: Object,
    },

    appealClosed_: {
      type: Boolean,
      value: function() {
        return !!(sessionStorage.appealClosed_ || localStorage.appealClosed_);
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
  },

  listeners: {
    'sideNav.iron-activate': 'onIronActivate_',
  },

  /** @private */
  onCloseAppealTap_: function() {
    sessionStorage.appealClosed_ = this.appealClosed_ = true;
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

  /** @override */
  ready: function() {
    this.$$('cr-toolbar').addEventListener('search-changed', function(e) {
      this.$$('settings-main').searchContents(e.detail);
    }.bind(this));
  },

  /** @private */
  directionDelegateChanged_: function() {
    this.$$('app-drawer').align = this.directionDelegate.isRtl() ?
        'right' : 'left';
  },
});
