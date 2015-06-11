// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



/**
 * 'cr-settings-appearance-page' is the settings page containing appearance
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <cr-settings-appearance-page prefs="{{prefs}}">
 *      </cr-settings-appearance-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-appearance-page
 */
Polymer({
  is: 'cr-settings-appearance-page',

  /** @override */
  attached: function() {
    // Query the initial state.
    cr.sendWithCallback('getResetThemeEnabled', undefined,
                        this.setResetThemeEnabled.bind(this));

    // Set up the change event listener.
    cr.addWebUIListener('reset-theme-enabled-changed',
                        this.setResetThemeEnabled.bind(this));
  },

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Route for the page.
     */
    route: String,

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: false,
      readOnly: true,
    },

    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'appearance',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() {
        return loadTimeData.getString('appearancePageTitle');
      },
    },

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: {
      type: String,
      value: 'home',
      readOnly: true,
    },
  },

  setResetThemeEnabled: function(enabled) {
    this.$.resetTheme.disabled = !enabled;
  },

  /** @private */
  openThemesGallery_: function() {
    window.open(loadTimeData.getString('themesGalleryUrl'));
  },

  /** @private */
  resetTheme_: function() {
    chrome.send('resetTheme');
  },
});
