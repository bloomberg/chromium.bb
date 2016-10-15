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
 */
Polymer({
  is: 'settings-appearance-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @private {!settings.AppearanceBrowserProxy} */
    browserProxy_: Object,

    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    useSystemTheme_: {
      type: Boolean,
      value: false,  // Can only be true on Linux, but value exists everywhere.
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

    /**
     * Dictionary defining page visibility.
     * @type {!AppearancePageVisibility}
     */
    pageVisibility: Object,
  },

  /** @private {string} */
  themeUrl_: '',

  observers: [
    'themeChanged_(prefs.extensions.theme.id.value, useSystemTheme_)',

<if expr="is_linux and not chromeos">
    // NOTE: this pref only exists on Linux.
    'useSystemThemePrefChanged_(prefs.extensions.theme.use_system.value)',
</if>
  ],

  created: function() {
    this.browserProxy_ = settings.AppearanceBrowserProxyImpl.getInstance();
  },

  ready: function() {
    this.$.defaultFontSize.menuOptions = this.fontSizeOptions_;
    // TODO(dschuyler): Look into adding a listener for the
    // default zoom percent.
    chrome.settingsPrivate.getDefaultZoomPercent(function(value) {
      // TODO(dpapad): Non-integer values will cause no <option> to be selected
      // until crbug.com/655742 is addressed.
      this.$.zoomLevel.value = value;
    }.bind(this));
  },

  /**
   * @param {number} zoom
   * @return {number} A zoom easier read by users.
   */
  formatZoom_: function(zoom) {
    return Math.round(zoom * 100);
  },

  /**
   * @param {boolean} isNtp Whether to use the NTP as the home page.
   * @param {string} homepage If not using NTP, use this URL.
   * @return {string} The sub-label.
   * @private
   */
  getShowHomeSubLabel_: function(isNtp, homepage) {
    if (isNtp)
      return this.i18n('homePageNtp');
    return homepage || this.i18n('exampleDotCom');
  },

  /** @private */
  onCustomizeFontsTap_: function() {
    settings.navigateTo(settings.Route.FONTS);
  },

  /** @private */
  onThemesTap_: function() {
    window.open(this.themeUrl_ || loadTimeData.getString('themesGalleryUrl'));
  },

<if expr="chromeos">
  /**
   * ChromeOS only.
   * @private
   */
  openWallpaperManager_: function() {
    this.browserProxy_.openWallpaperManager();
  },
</if>

  /** @private */
  onUseDefaultTap_: function() {
    this.browserProxy_.useDefaultTheme();
  },

<if expr="is_linux and not chromeos">
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
    return !!themeId || !useSystemTheme;
  },

  /** @private */
  onUseSystemTap_: function() {
    this.browserProxy_.useSystemTheme();
  },
</if>

  /**
   * @param {string} themeId
   * @param {boolean} useSystemTheme
   * @private
   */
  themeChanged_: function(themeId, useSystemTheme) {
    if (themeId) {
      assert(!useSystemTheme);

      this.browserProxy_.getThemeInfo(themeId).then(function(info) {
        this.themeSublabel_ = info.name;
      }.bind(this));

      this.themeUrl_ = `https://chrome.google.com/webstore/detail/${themeId}`;
      return;
    }

    var i18nId;
<if expr="is_linux and not chromeos">
    i18nId = useSystemTheme ? 'systemTheme' : 'classicTheme';
</if>
<if expr="not is_linux or chromeos">
    i18nId = 'chooseFromWebStore';
</if>
    this.themeSublabel_ = this.i18n(i18nId);
    this.themeUrl_ = '';
  },

  /** @private */
  onZoomLevelChange_: function() {
    chrome.settingsPrivate.setDefaultZoomPercent(
        parseFloat(this.$.zoomLevel.value));
  },

  /**
   * @param {boolean} bookmarksBarVisible if bookmarks bar option is visible.
   * @return {string} 'first' if the argument is false or empty otherwise.
   * @private
   */
  getFirst_: function(bookmarksBarVisible) {
    return !bookmarksBarVisible ? 'first' : '';
  }
});
