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

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Translated strings used in data binding.
     */
    i18n_: {
      type: Object,
      value: function() {
        return {
          homePageNtp: loadTimeData.getString('homePageNtp'),
          openThisPage: loadTimeData.getString('openThisPage'),
        };
      },
    },
  },

  /** @override */
  attached: function() {
    // Query the initial state.
    cr.sendWithCallback('getResetThemeEnabled', undefined,
                        this.setResetThemeEnabled.bind(this));

    // Set up the change event listener.
    cr.addWebUIListener('reset-theme-enabled-changed',
                        this.setResetThemeEnabled.bind(this));
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
