// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-page' is the settings page containing search settings.
 */
Polymer({
  is: 'settings-search-page',

  behaviors: [I18nBehavior],

  properties: {
    prefs: Object,

    /**
     * List of default search engines available.
     * @private {!Array<!SearchEngine>}
     */
    searchEngines_: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /** @private {!SearchPageHotwordInfo|undefined} */
    hotwordInfo_: Object,

    /**
     * This is a local PrefObject used to reflect the enabled state of hotword
     * search. It is not tied directly to a pref. (There are two prefs
     * associated with  state and they do not map directly to whether or not
     * hotword search is actually enabled).
     * @private {!chrome.settingsPrivate.PrefObject|undefined}
     */
    hotwordSearchEnablePref_: Object,

    /** @private */
    googleNowAvailable_: Boolean,
  },

  /** @private {?settings.SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    // Omnibox search engine
    var updateSearchEngines = function(searchEngines) {
      this.set('searchEngines_', searchEngines.defaults);
    }.bind(this);
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    cr.addWebUIListener('search-engines-changed', updateSearchEngines);

    // Hotword (OK Google)
    cr.addWebUIListener(
        'hotword-info-update', this.hotwordInfoUpdate_.bind(this));
    this.browserProxy_.getHotwordInfo().then(function(hotwordInfo) {
      this.hotwordInfoUpdate_(hotwordInfo);
    }.bind(this));

    // Google Now cards in the launcher
    cr.addWebUIListener(
        'google-now-availability-changed',
        this.googleNowAvailabilityUpdate_.bind(this));
    this.browserProxy_.getGoogleNowAvailability().then(function(available) {
        this.googleNowAvailabilityUpdate_(available);
    }.bind(this));
  },

  /** @private */
  onChange_: function() {
    var select = /** @type {!HTMLSelectElement} */ (this.$$('select'));
    var searchEngine = this.searchEngines_[select.selectedIndex];
    this.browserProxy_.setDefaultSearchEngine(searchEngine.modelIndex);
  },

  /** @private */
  onDisableExtension_: function() {
    this.fire('refresh-pref', 'default_search_provider.enabled');
  },

  /** @private */
  onManageSearchEnginesTap_: function() {
    settings.navigateTo(settings.Route.SEARCH_ENGINES);
  },

  /**
   * @param {Event} event
   * @private
   */
  onHotwordSearchEnableChange_: function(event) {
    // Do not set the pref directly, allow Chrome to run the setup app instead.
    this.browserProxy_.setHotwordSearchEnabled(
        !!this.hotwordSearchEnablePref_.value);
  },

  /**
   * @param {!SearchPageHotwordInfo} hotwordInfo
   * @private
   */
  hotwordInfoUpdate_: function(hotwordInfo) {
    this.hotwordInfo_ = hotwordInfo;
    this.hotwordSearchEnablePref_ = {
      key: 'unused',  // required for PrefObject
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: this.hotwordInfo_.enabled,
    };
  },

  /**
   * @param {boolean} available
   * @private
   */
  googleNowAvailabilityUpdate_: function(available) {
    this.googleNowAvailable_ = available;
  },

  /**
   * @return {string}
   * @private
   */
  getHotwordSearchEnableSubLabel_: function() {
    return this.i18n(
        this.hotwordInfo_.alwaysOn ? 'searchOkGoogleSubtextAlwaysOn' :
                                     'searchOkGoogleSubtextNoHardware');
  },

  /**
   * @return {boolean}
   * @private
   */
  getShowHotwordSearchRetrain_: function() {
    return this.hotwordInfo_.enabled && this.hotwordInfo_.alwaysOn;
  },

  /**
   * @return {boolean} True if the pref is enabled but hotword is not.
   * @private
   */
  getShowHotwordError_: function() {
    return this.hotwordInfo_.enabled && !!this.hotwordInfo_.errorMessage;
  },

  /** @private */
  onRetrainTap_: function() {
    // Re-enable hotword search enable; this will trigger the retrain UI.
    this.browserProxy_.setHotwordSearchEnabled(this.hotwordInfo_.enabled);
  },

  /** @private */
  onManageAudioHistoryTap_: function() {
    window.open(loadTimeData.getString('manageAudioHistoryUrl'));
  },

  /**
   * @param {Event} event
   * @private
   */
  doNothing_: function(event) {
    event.stopPropagation();
  }
});
