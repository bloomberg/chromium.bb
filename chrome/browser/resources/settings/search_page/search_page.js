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

    // <if expr="chromeos">
    arcEnabled: Boolean,

    voiceInteractionValuePropAccepted: Boolean,
    // </if>

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

    /** @private Filter applied to search engines. */
    searchEnginesFilter_: String,

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

    /** @type {?Map<string, string>} */
    focusConfig_: Object,

    // <if expr="chromeos">
    /** @private */
    voiceInteractionFeatureEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableVoiceInteraction');
      },
    },

    /** @private */
    assistantOn_: {
      type: Boolean,
      computed:
          'isAssistantTurnedOn_(arcEnabled, voiceInteractionValuePropAccepted)',
    }
    // </if>
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
    var updateSearchEngines = searchEngines => {
      this.set('searchEngines_', searchEngines.defaults);
      this.requestHotwordInfoUpdate_();
    };
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    cr.addWebUIListener('search-engines-changed', updateSearchEngines);

    // Hotword (OK Google) listener
    cr.addWebUIListener(
        'hotword-info-update', this.hotwordInfoUpdate_.bind(this));

    this.focusConfig_ = new Map();
    if (settings.routes.SEARCH_ENGINES) {
      this.focusConfig_.set(
          settings.routes.SEARCH_ENGINES.path,
          '#engines-subpage-trigger .subpage-arrow');
    }
    // <if expr="chromeos">
    if (settings.routes.GOOGLE_ASSISTANT) {
      this.focusConfig_.set(
          settings.routes.GOOGLE_ASSISTANT.path,
          '#assistant-subpage-trigger .subpage-arrow');
    }
    // </if>
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
    settings.navigateTo(settings.routes.SEARCH_ENGINES);
  },

  // <if expr="chromeos">
  /** @private */
  onGoogleAssistantTap_: function() {
    assert(this.voiceInteractionFeatureEnabled_);

    if (!this.assistantOn_) {
      return;
    }

    settings.navigateTo(settings.routes.GOOGLE_ASSISTANT);
  },

  /** @private */
  onAssistantTurnOnTap_: function(event) {
    this.browserProxy_.turnOnGoogleAssistant();
  },
  // </if>

  /**
   * @param {!Event} event
   * @private
   */
  onHotwordSearchEnableChange_: function(event) {
    // Do not set the pref directly, allow Chrome to run the setup app instead.
    this.browserProxy_.setHotwordSearchEnabled(
        !!this.hotwordSearchEnablePref_.value);
  },

  /** @private */
  requestHotwordInfoUpdate_: function() {
    this.browserProxy_.getHotwordInfo().then(hotwordInfo => {
      this.hotwordInfoUpdate_(hotwordInfo);
    });
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

  // <if expr="chromeos">
  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAssistantEnabledDisabledLabel_: function(toggleValue) {
    return this.i18n(
        toggleValue ? 'searchGoogleAssistantEnabled' :
                      'searchGoogleAssistantDisabled');
  },

  /** @private
   *  @param {boolean} arcEnabled
   *  @param {boolean} valuePropAccepted
   *  @return {boolean}
   */
  isAssistantTurnedOn_: function(arcEnabled, valuePropAccepted) {
    return arcEnabled && valuePropAccepted;
  },
  // </if>

  /**
   * @param {!Event} event
   * @private
   */
  doNothing_: function(event) {
    event.stopPropagation();
  },

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchControlledByPolicy_: function(pref) {
    return pref.controlledBy == chrome.settingsPrivate.ControlledBy.USER_POLICY;
  },

  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean}
   * @private
   */
  isDefaultSearchEngineEnforced_: function(pref) {
    return pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },
});
