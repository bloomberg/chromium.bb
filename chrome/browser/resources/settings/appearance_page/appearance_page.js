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
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
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
     * @private
     */
    defaultZoomLevel_: {
      notify: true,
      type: Object,
      value: function() {
        return {
          type: chrome.settingsPrivate.PrefType.NUMBER,
        };
      },
    },

    /**
     * List of options for the font size drop-down menu.
     * @type {!DropdownMenuOptionList}
     */
    fontSizeOptions_: {
      readOnly: true,
      type: Array,
      value: function() {
        return [
          {value: 9, name: loadTimeData.getString('verySmall')},
          {value: 12, name: loadTimeData.getString('small')},
          {value: 16, name: loadTimeData.getString('medium')},
          {value: 20, name: loadTimeData.getString('large')},
          {value: 24, name: loadTimeData.getString('veryLarge')},
        ];
      },
    },

    /**
     * List of options for the page zoom drop-down menu.
     * @type {!DropdownMenuOptionList}
     */
    pageZoomOptions_: {
      readOnly: true,
      type: Array,
      value: [
        {value: 25, name: '25%'},
        {value: 33, name: '33%'},
        {value: 50, name: '50%'},
        {value: 67, name: '67%'},
        {value: 75, name: '75%'},
        {value: 90, name: '90%'},
        {value: 100, name: '100%'},
        {value: 110, name: '110%'},
        {value: 125, name: '125%'},
        {value: 150, name: '150%'},
        {value: 175, name: '175%'},
        {value: 200, name: '200%'},
        {value: 300, name: '300%'},
        {value: 400, name: '400%'},
        {value: 500, name: '500%'},
      ],
    },
  },

  behaviors: [
    I18nBehavior,
  ],

  observers: [
    'zoomLevelChanged_(defaultZoomLevel_.value)',
  ],

  ready: function() {
    this.$.defaultFontSize.menuOptions = this.fontSizeOptions_;
    this.$.pageZoom.menuOptions = this.pageZoomOptions_;
    // TODO(dschuyler): Look into adding a listener for the
    // default zoom percent.
    chrome.settingsPrivate.getDefaultZoomPercent(
        this.zoomPrefChanged_.bind(this));
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

  /** @private */
  showFontsPage_: function() {
    return this.currentRoute.subpage[0] == 'appearance-fonts';
  },

  /**
   * @param {number} percent The integer percentage of the page zoom.
   * @private
   */
  zoomPrefChanged_: function(percent) {
    this.set('defaultZoomLevel_.value', percent);
  },

  /**
   * @param {number} percent The integer percentage of the page zoom.
   * @private
   */
  zoomLevelChanged_: function(percent) {
    // The |percent| may be undefined on startup.
    if (percent === undefined)
      return;
    chrome.settingsPrivate.setDefaultZoomPercent(percent);
  },
});
