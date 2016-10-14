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

    /** @private */
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
        {value: 80, name: '80%'},
        {value: 90, name: '90%'},
        {value: 100, name: '100%'},
        {value: 110, name: '110%'},
        {value: 125, name: '125%'},
        {value: 150, name: '150%'},
        {value: 175, name: '175%'},
        {value: 200, name: '200%'},
        {value: 250, name: '250%'},
        {value: 300, name: '300%'},
        {value: 400, name: '400%'},
        {value: 500, name: '500%'},
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

    'zoomLevelChanged_(defaultZoomLevel_.value)',
  ],

  created: function() {
    this.browserProxy_ = settings.AppearanceBrowserProxyImpl.getInstance();
  },

  ready: function() {
    this.$.defaultFontSize.menuOptions = this.fontSizeOptions_;
    this.$.pageZoom.menuOptions = this.pageZoomOptions_;
    // TODO(dschuyler): Look into adding a listener for the
    // default zoom percent.
    chrome.settingsPrivate.getDefaultZoomPercent(
        this.zoomPrefChanged_.bind(this));
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

  /**
   * @param {boolean} bookmarksBarVisible if bookmarks bar option is visible.
   * @return {string} 'first' if the argument is false or empty otherwise.
   * @private
   */
  getFirst_: function(bookmarksBarVisible) {
    return !bookmarksBarVisible ? 'first' : '';
  }
});
