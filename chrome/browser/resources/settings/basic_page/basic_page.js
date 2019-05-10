// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-basic-page' is the settings page containing the actual settings.
 */
Polymer({
  is: 'settings-basic-page',

  behaviors: [
    settings.MainPageBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    // <if expr="chromeos">
    showApps: Boolean,

    showAndroidApps: Boolean,

    showCrostini: Boolean,

    allowCrostini_: Boolean,

    havePlayStoreApp: Boolean,
    // </if>

    /** @type {!AndroidAppsInfo|undefined} */
    androidAppsInfo: Object,

    showChangePassword: {
      type: Boolean,
      value: false,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!PageVisibility}
     */
    pageVisibility: {
      type: Object,
      value: function() {
        return {};
      },
    },

    advancedToggleExpanded: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'advancedToggleExpandedChanged_',
    },

    /**
     * True if a section is fully expanded to hide other sections beneath it.
     * False otherwise (even while animating a section open/closed).
     * @private {boolean}
     */
    hasExpandedSection_: {
      type: Boolean,
      value: false,
    },

    /**
     * True if the basic page should currently display the reset profile banner.
     * @private {boolean}
     */
    showResetProfileBanner_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showResetProfileBanner');
      },
    },

    // <if expr="chromeos">
    /**
     * Whether the user is a secondary user. Computed so that it is calculated
     * correctly after loadTimeData is available.
     * @private
     */
    showSecondaryUserBanner_: {
      type: Boolean,
      computed: 'computeShowSecondaryUserBanner_(hasExpandedSection_)',
    },
    // </if>

    /** @private {!settings.Route|undefined} */
    currentRoute_: Object,
  },

  hostAttributes: {
    role: 'main',
  },

  listeners: {
    'subpage-expand': 'onSubpageExpanded_',
  },

  /**
   * Used to avoid handling a new toggle while currently toggling.
   * @private {boolean}
   */
  advancedTogglingInProgress_: false,

  /** @override */
  attached: function() {
    this.currentRoute_ = settings.getCurrentRoute();

    this.allowCrostini_ = loadTimeData.valueExists('allowCrostini') &&
        loadTimeData.getBoolean('allowCrostini');

    this.addWebUIListener('change-password-visibility', visibility => {
      this.showChangePassword = visibility;
    });

    if (loadTimeData.getBoolean('passwordProtectionAvailable')) {
      settings.ChangePasswordBrowserProxyImpl.getInstance()
          .initializeChangePasswordHandler();
    }

    if (settings.AndroidAppsBrowserProxyImpl) {
      this.addWebUIListener(
          'android-apps-info-update', this.androidAppsInfoUpdate_.bind(this));
      settings.AndroidAppsBrowserProxyImpl.getInstance()
          .requestAndroidAppsInfo();
    }
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    this.currentRoute_ = newRoute;

    if (settings.routes.ADVANCED &&
        settings.routes.ADVANCED.contains(newRoute)) {
      this.advancedToggleExpanded = true;
    }

    if (oldRoute && oldRoute.isSubpage()) {
      // If the new route isn't the same expanded section, reset
      // hasExpandedSection_ for the next transition.
      if (!newRoute.isSubpage() || newRoute.section != oldRoute.section) {
        this.hasExpandedSection_ = false;
      }
    } else {
      assert(!this.hasExpandedSection_);
    }

    settings.MainPageBehavior.currentRouteChanged.call(
        this, newRoute, oldRoute);
  },

  // Override settings.MainPageBehavior method.
  containsRoute: function(route) {
    return !route || settings.routes.BASIC.contains(route) ||
        settings.routes.ADVANCED.contains(route);
  },

  /**
   * @param {boolean|undefined} visibility
   * @return {boolean}
   * @private
   */
  showPage_: function(visibility) {
    return visibility !== false;
  },

  /**
   * Queues a task to search the basic sections, then another for the advanced
   * sections.
   * @param {string} query The text to search for.
   * @return {!Promise<!settings.SearchResult>} A signal indicating that
   *     searching finished.
   */
  searchContents: function(query) {
    const whenSearchDone = [
      settings.getSearchManager().search(query, assert(this.$$('#basicPage'))),
    ];

    if (this.pageVisibility.advancedSettings !== false) {
      whenSearchDone.push(
          this.$$('#advancedPageTemplate').get().then(function(advancedPage) {
            return settings.getSearchManager().search(query, advancedPage);
          }));
    }

    return Promise.all(whenSearchDone).then(function(requests) {
      // Combine the SearchRequests results to a single SearchResult object.
      return {
        canceled: requests.some(function(r) {
          return r.canceled;
        }),
        didFindMatches: requests.some(function(r) {
          return r.didFindMatches();
        }),
        // All requests correspond to the same user query, so only need to check
        // one of them.
        wasClearSearch: requests[0].isSame(''),
      };
    });
  },

  // <if expr="chromeos">
  /**
   * @return {boolean}
   * @private
   */
  computeShowSecondaryUserBanner_: function() {
    return !this.hasExpandedSection_ &&
        loadTimeData.getBoolean('isSecondaryUser');
  },
  // </if>

  /** @private */
  onResetProfileBannerClosed_: function() {
    this.showResetProfileBanner_ = false;
  },

  /**
   * @param {!AndroidAppsInfo} info
   * @private
   */
  androidAppsInfoUpdate_: function(info) {
    this.androidAppsInfo = info;
  },

  /**
   * Returns true in case Android apps settings needs to be created. It is not
   * created in case ARC++ is not allowed for the current profile.
   * @return {boolean}
   * @private
   */
  shouldCreateAndroidAppsSection_: function() {
    const visibility = /** @type {boolean|undefined} */ (
        this.get('pageVisibility.androidApps'));
    return this.showAndroidApps && this.showPage_(visibility);
  },

  /**
   * Returns true in case Android apps settings should be shown. It is not
   * shown in case we don't have the Play Store app and settings app is not
   * yet available.
   * @return {boolean}
   * @private
   */
  shouldShowAndroidAppsSection_: function() {
    if (this.havePlayStoreApp ||
        (this.androidAppsInfo && this.androidAppsInfo.settingsAppAvailable)) {
      return true;
    }
    return false;
  },

  /**
   * Hides everything but the newly expanded subpage.
   * @private
   */
  onSubpageExpanded_: function() {
    this.hasExpandedSection_ = true;
  },

  /**
   * Render the advanced page now (don't wait for idle).
   * @private
   */
  advancedToggleExpandedChanged_: function() {
    if (!this.advancedToggleExpanded) {
      return;
    }

    // In Polymer2, async() does not wait long enough for layout to complete.
    // Polymer.RenderStatus.beforeNextRender() must be used instead.
    Polymer.RenderStatus.beforeNextRender(this, () => {
      this.$$('#advancedPageTemplate').get();
    });
  },

  advancedToggleClicked_: function() {
    if (this.advancedTogglingInProgress_) {
      return;
    }

    this.advancedTogglingInProgress_ = true;
    const toggle = this.$$('#toggleContainer');
    if (!this.advancedToggleExpanded) {
      this.advancedToggleExpanded = true;
      this.async(() => {
        this.$$('#advancedPageTemplate').get().then(() => {
          this.fire('scroll-to-top', {
            top: toggle.offsetTop,
            callback: () => {
              this.advancedTogglingInProgress_ = false;
            }
          });
        });
      });
    } else {
      this.fire('scroll-to-bottom', {
        bottom: toggle.offsetTop + toggle.offsetHeight + 24,
        callback: () => {
          this.advancedToggleExpanded = false;
          this.advancedTogglingInProgress_ = false;
        }
      });
    }
  },

  /**
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @return {boolean}
   * @private
   */
  showAdvancedToggle_: function(inSearchMode, hasExpandedSection) {
    return !inSearchMode && !hasExpandedSection;
  },

  /**
   * @param {!settings.Route} currentRoute
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @return {boolean} Whether to show the basic page, taking into account
   *     both routing and search state.
   * @private
   */
  showBasicPage_: function(currentRoute, inSearchMode, hasExpandedSection) {
    return !hasExpandedSection || settings.routes.BASIC.contains(currentRoute);
  },

  /**
   * @param {!settings.Route} currentRoute
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @param {boolean} advancedToggleExpanded
   * @return {boolean} Whether to show the advanced page, taking into account
   *     both routing and search state.
   * @private
   */
  showAdvancedPage_: function(
      currentRoute, inSearchMode, hasExpandedSection, advancedToggleExpanded) {
    return hasExpandedSection ?
        (settings.routes.ADVANCED &&
         settings.routes.ADVANCED.contains(currentRoute)) :
        advancedToggleExpanded || inSearchMode;
  },

  /**
   * @param {(boolean|undefined)} visibility
   * @return {boolean} True unless visibility is false.
   * @private
   */
  showAdvancedSettings_: function(visibility) {
    return visibility !== false;
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Icon name.
   * @private
   */
  getArrowIcon_: function(opened) {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  },

  /**
   * @param {boolean} bool
   * @return {string}
   * @private
   */
  boolToString_: function(bool) {
    return bool.toString();
  },
});
