// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * This is the absolute difference maintained between standard and
 * fixed-width font sizes. http://crbug.com/91922.
 * @type {number}
 */
const SIZE_DIFFERENCE_FIXED_STANDARD = 3;


/**
 * 'settings-appearance-page' is the settings page containing appearance
 * settings.
 */
Polymer({
  is: 'settings-appearance-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Dictionary defining page visibility.
     * @type {!AppearancePageVisibility}
     */
    pageVisibility: Object,

    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    defaultZoom_: Number,

    /** @private */
    isWallpaperPolicyControlled_: {type: Boolean, value: true},

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
     * @type {!Array<number>}
     */
    pageZoomLevels_: {
      readOnly: true,
      type: Array,
      value: [
        // TODO(dbeam): get these dynamically from C++ instead.
        1 / 4,
        1 / 3,
        1 / 2,
        2 / 3,
        3 / 4,
        4 / 5,
        9 / 10,
        1,
        11 / 10,
        5 / 4,
        3 / 2,
        7 / 4,
        2,
        5 / 2,
        3,
        4,
        5,
      ],
    },

    /** @private */
    themeSublabel_: String,

    /** @private */
    themeUrl_: String,

    /** @private */
    useSystemTheme_: {
      type: Boolean,
      value: false,  // Can only be true on Linux, but value exists everywhere.
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.FONTS) {
          map.set(
              settings.routes.FONTS.path, '#customize-fonts-subpage-trigger');
        }
        return map;
      },
    },
  },

  /** @private {?settings.AppearanceBrowserProxy} */
  browserProxy_: null,

  observers: [
    'defaultFontSizeChanged_(prefs.webkit.webprefs.default_font_size.value)',
    'themeChanged_(prefs.extensions.theme.id.value, useSystemTheme_)',

    // <if expr="is_linux and not chromeos">
    // NOTE: this pref only exists on Linux.
    'useSystemThemePrefChanged_(prefs.extensions.theme.use_system.value)',
    // </if>
  ],

  /** @override */
  created: function() {
    this.browserProxy_ = settings.AppearanceBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.$.defaultFontSize.menuOptions = this.fontSizeOptions_;
    // TODO(dschuyler): Look into adding a listener for the
    // default zoom percent.
    this.browserProxy_.getDefaultZoom().then(zoom => {
      this.defaultZoom_ = zoom;
    });
    // <if expr="chromeos">
    this.browserProxy_.isWallpaperSettingVisible().then(
        isWallpaperSettingVisible => {
          assert(this.pageVisibility);
          this.pageVisibility.setWallpaper = isWallpaperSettingVisible;
        });
    this.browserProxy_.isWallpaperPolicyControlled().then(
        isPolicyControlled => {
          this.isWallpaperPolicyControlled_ = isPolicyControlled;
        });
    // </if>
  },

  /**
   * @param {number} zoom
   * @return {number} A zoom easier read by users.
   * @private
   */
  formatZoom_: function(zoom) {
    return Math.round(zoom * 100);
  },

  /**
   * @param {boolean} showHomepage Whether to show home page.
   * @param {boolean} isNtp Whether to use the NTP as the home page.
   * @param {string} homepageValue If not using NTP, use this URL.
   * @return {string} The sub-label.
   * @private
   */
  getShowHomeSubLabel_: function(showHomepage, isNtp, homepageValue) {
    if (!showHomepage)
      return this.i18n('homeButtonDisabled');
    if (isNtp)
      return this.i18n('homePageNtp');
    return homepageValue || this.i18n('customWebAddress');
  },

  /** @private */
  onCustomizeFontsTap_: function() {
    settings.navigateTo(settings.routes.FONTS);
  },

  /** @private */
  onDisableExtension_: function() {
    this.fire('refresh-pref', 'homepage');
  },

  /**
   * @param {number} value The changed font size slider value.
   * @private
   */
  defaultFontSizeChanged_: function(value) {
    // This pref is handled separately in some extensions, but here it is tied
    // to default_font_size (to simplify the UI).
    this.set(
        'prefs.webkit.webprefs.default_fixed_font_size.value',
        value - SIZE_DIFFERENCE_FIXED_STANDARD);
  },

  /**
   * Open URL for either current theme or the theme gallery.
   * @private
   */
  openThemeUrl_: function() {
    window.open(this.themeUrl_ || loadTimeData.getString('themesGalleryUrl'));
  },

  // <if expr="chromeos">
  /**
   * ChromeOS only.
   * @private
   */
  openWallpaperManager_: function() {
    this.browserProxy_.openWallpaperManager();
  },
  // </if>

  /** @private */
  onUseDefaultTap_: function() {
    this.browserProxy_.useDefaultTheme();
  },

  // <if expr="is_linux and not chromeos">
  /**
   * @param {boolean} useSystemTheme
   * @private
   */
  useSystemThemePrefChanged_: function(useSystemTheme) {
    this.useSystemTheme_ = useSystemTheme;
  },

  /**
   * @param {string} themeId
   * @param {boolean} useSystemTheme
   * @return {boolean} Whether to show the "USE CLASSIC" button.
   * @private
   */
  showUseClassic_: function(themeId, useSystemTheme) {
    return !!themeId || useSystemTheme;
  },

  /**
   * @param {string} themeId
   * @param {boolean} useSystemTheme
   * @return {boolean} Whether to show the "USE GTK+" button.
   * @private
   */
  showUseSystem_: function(themeId, useSystemTheme) {
    return (!!themeId || !useSystemTheme) && !this.browserProxy_.isSupervised();
  },

  /**
   * @param {string} themeId
   * @param {boolean} useSystemTheme
   * @return {boolean} Whether to show the secondary area where "USE CLASSIC"
   *     and "USE GTK+" buttons live.
   * @private
   */
  showThemesSecondary_: function(themeId, useSystemTheme) {
    return this.showUseClassic_(themeId, useSystemTheme) ||
        this.showUseSystem_(themeId, useSystemTheme);
  },

  /** @private */
  onUseSystemTap_: function() {
    this.browserProxy_.useSystemTheme();
  },
  // </if>

  /**
   * @param {string} themeId
   * @param {boolean} useSystemTheme
   * @private
   */
  themeChanged_: function(themeId, useSystemTheme) {
    if (themeId) {
      assert(!useSystemTheme);

      this.browserProxy_.getThemeInfo(themeId).then(info => {
        this.themeSublabel_ = info.name;
      });

      this.themeUrl_ = 'https://chrome.google.com/webstore/detail/' + themeId;
      return;
    }

    let i18nId;
    // <if expr="is_linux and not chromeos">
    i18nId = useSystemTheme ? 'systemTheme' : 'classicTheme';
    // </if>
    // <if expr="not is_linux or chromeos">
    i18nId = 'chooseFromWebStore';
    // </if>
    this.themeSublabel_ = this.i18n(i18nId);
    this.themeUrl_ = '';
  },

  /** @private */
  onZoomLevelChange_: function() {
    chrome.settingsPrivate.setDefaultZoom(parseFloat(this.$.zoomLevel.value));
  },

  /**
   * @param {boolean} bookmarksBarVisible if bookmarks bar option is visible.
   * @return {string} 'first' if the argument is false or empty otherwise.
   * @private
   */
  getFirst_: function(bookmarksBarVisible) {
    return !bookmarksBarVisible ? 'first' : '';
  },

  /**
   * @see content::ZoomValuesEqual().
   * @param {number} zoom1
   * @param {number} zoom2
   * @return {boolean}
   * @private
   */
  zoomValuesEqual_: function(zoom1, zoom2) {
    return Math.abs(zoom1 - zoom2) <= 0.001;
  },
});
})();
