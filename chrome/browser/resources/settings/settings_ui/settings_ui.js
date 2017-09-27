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
assert(
    !settings.defaultResourceLoaded,
    'settings_ui.js run twice. You probably have an invalid import.');
/** Global defined when the main Settings script runs. */
settings.defaultResourceLoaded = true;

Polymer({
  is: 'settings-ui',

  behaviors: [settings.RouteObserverBehavior, CrContainerShadowBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: Object,

    /** @private */
    advancedOpened_: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /** @private {boolean} */
    toolbarSpinnerActive_: {
      type: Boolean,
      value: false,
    },

    /**
     * @private {!GuestModePageVisibility}
     */
    pageVisibility_: {type: Object, value: settings.pageVisibility},

    /** @private */
    showAndroidApps_: Boolean,

    /** @private */
    showMultidevice_: Boolean,

    /** @private */
    havePlayStoreApp_: Boolean,

    /** @private */
    lastSearchQuery_: {
      type: String,
      value: '',
    }
  },

  listeners: {
    'refresh-pref': 'onRefreshPref_',
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
    // Lazy-create the drawer the first time it is opened or swiped into view.
    listenOnce(this.$.drawer, 'open-changed', () => {
      this.$.drawerTemplate.if = true;
    });

    window.addEventListener('popstate', e => {
      this.$.drawer.closeDrawer();
    });

    CrPolicyStrings = {
      controlledSettingExtension:
          loadTimeData.getString('controlledSettingExtension'),
      controlledSettingPolicy:
          loadTimeData.getString('controlledSettingPolicy'),
      controlledSettingRecommendedMatches:
          loadTimeData.getString('controlledSettingRecommendedMatches'),
      controlledSettingRecommendedDiffers:
          loadTimeData.getString('controlledSettingRecommendedDiffers'),
      // <if expr="chromeos">
      controlledSettingShared:
          loadTimeData.getString('controlledSettingShared'),
      controlledSettingOwner: loadTimeData.getString('controlledSettingOwner'),
      // </if>
    };

    // <if expr="chromeos">
    CrOncStrings = {
      OncTypeCellular: loadTimeData.getString('OncTypeCellular'),
      OncTypeEthernet: loadTimeData.getString('OncTypeEthernet'),
      OncTypeTether: loadTimeData.getString('OncTypeTether'),
      OncTypeVPN: loadTimeData.getString('OncTypeVPN'),
      OncTypeWiFi: loadTimeData.getString('OncTypeWiFi'),
      OncTypeWiMAX: loadTimeData.getString('OncTypeWiMAX'),
      networkListItemConnected:
          loadTimeData.getString('networkListItemConnected'),
      networkListItemConnecting:
          loadTimeData.getString('networkListItemConnecting'),
      networkListItemConnectingTo:
          loadTimeData.getString('networkListItemConnectingTo'),
      networkListItemInitializing:
          loadTimeData.getString('networkListItemInitializing'),
      networkListItemNotConnected:
          loadTimeData.getString('networkListItemNotConnected'),
      vpnNameTemplate: loadTimeData.getString('vpnNameTemplate'),
    };
    // </if>

    this.showAndroidApps_ = loadTimeData.valueExists('androidAppsVisible') &&
        loadTimeData.getBoolean('androidAppsVisible');
    this.showMultidevice_ = this.showAndroidApps_ &&
        loadTimeData.valueExists('enableMultideviceSettings') &&
        loadTimeData.getBoolean('enableMultideviceSettings');
    this.havePlayStoreApp_ = loadTimeData.valueExists('havePlayStoreApp') &&
        loadTimeData.getBoolean('havePlayStoreApp');

    this.addEventListener('show-container', () => {
      this.$.container.style.visibility = 'visible';
    });

    this.addEventListener('hide-container', () => {
      this.$.container.style.visibility = 'hidden';
    });
  },

  /** @override */
  attached: function() {
    document.documentElement.classList.remove('loading');

    setTimeout(function() {
      chrome.send(
          'metricsHandler:recordTime',
          ['Settings.TimeUntilInteractive', window.performance.now()]);
    });

    // Preload bold Roboto so it doesn't load and flicker the first time used.
    document.fonts.load('bold 12px Roboto');
    settings.setGlobalScrollTarget(this.$.container);
  },

  /** @override */
  detached: function() {
    settings.resetRouteForTesting();
  },

  /** @param {!settings.Route} route */
  currentRouteChanged: function(route) {
    var urlSearchQuery = settings.getQueryParameters().get('search') || '';
    if (urlSearchQuery == this.lastSearchQuery_)
      return;

    this.lastSearchQuery_ = urlSearchQuery;

    var toolbar = /** @type {!CrToolbarElement} */ (this.$$('cr-toolbar'));
    var searchField =
        /** @type {CrToolbarSearchFieldElement} */ (toolbar.getSearchField());

    // If the search was initiated by directly entering a search URL, need to
    // sync the URL parameter to the textbox.
    if (urlSearchQuery != searchField.getValue()) {
      // Setting the search box value without triggering a 'search-changed'
      // event, to prevent an unnecessary duplicate entry in |window.history|.
      searchField.setValue(urlSearchQuery, true /* noEvent */);
    }

    this.$.main.searchContents(urlSearchQuery);
  },

  /**
   * @param {!CustomEvent} e
   * @private
   */
  onRefreshPref_: function(e) {
    var prefName = /** @type {string} */ (e.detail);
    return /** @type {SettingsPrefsElement} */ (this.$.prefs).refresh(prefName);
  },

  /**
   * Handles the 'search-changed' event fired from the toolbar.
   * @param {!Event} e
   * @private
   */
  onSearchChanged_: function(e) {
    // Trim leading whitespace only, to prevent searching for empty string. This
    // still allows the user to search for 'foo bar', while taking a long pause
    // after typing 'foo '.
    var query = e.detail.replace(/^\s+/, '');
    // Prevent duplicate history entries.
    if (query == this.lastSearchQuery_)
      return;

    settings.navigateTo(
        settings.routes.BASIC,
        query.length > 0 ?
            new URLSearchParams('search=' + encodeURIComponent(query)) :
            undefined,
        /* removeSearch */ true);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onIronActivate_: function(event) {
    if (event.detail.item.id != 'advancedSubmenu')
      this.$.drawer.closeDrawer();
  },

  /** @private */
  onMenuButtonTap_: function() {
    this.$.drawer.toggle();
  },

  /** @private */
  onMenuClosed_: function() {
    // Add tab index so that the container can be focused.
    this.$.container.setAttribute('tabindex', '-1');
    this.$.container.focus();

    listenOnce(this.$.container, ['blur', 'pointerdown'], () => {
      this.$.container.removeAttribute('tabindex');
    });
  },
});
