// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'settings-appearance-page' is the settings page containing appearance
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-appearance-page prefs="{{prefs}}">
 *      </settings-appearance-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-appearance-page
 */
Polymer({
  is: 'settings-appearance-page',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      notify: true,
      type: Object,
    },

    /**
     * @private
     */
    allowResetTheme_: {
      notify: true,
      type: Boolean,
      value: false,
    },

    /**
     * List of options for the font size drop-down menu.
     * The order of entries in this array matches the
     * prefs.browser.clear_data.time_period.value enum.
     * @private {!Array<!Array<{0: number, 1: string}>>}
     */
    fontSizeOptions_: {
      readOnly: true,
      type: Array,
      value: function() {
        return [
          [9, loadTimeData.getString('verySmall')],
          [12, loadTimeData.getString('small')],
          [16, loadTimeData.getString('medium')],
          [20, loadTimeData.getString('large')],
          [24, loadTimeData.getString('veryLarge')],
        ];
      },
    },
  },

  behaviors: [
    I18nBehavior,
  ],

  ready: function() {
    this.$.defaultFontSize.menuOptions = this.fontSizeOptions_;
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

  /**
   * @param {boolean} enabled Whether the theme reset is available.
   */
  setResetThemeEnabled: function(enabled) {
    this.allowResetTheme_ = enabled;
  },

  /** @private */
  onCustomizeFontsTap_: function() {
    this.$.pages.setSubpageChain(['appearance-fonts']);
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
