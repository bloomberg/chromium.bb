// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-settings-languages' provides convenient access to
 * Chrome's language and input method settings.
 *
 * Instances of this element have a 'languages' property, which reflects the
 * current language settings. The 'languages' property is read-only, meaning
 * hosts using this element cannot change it directly. Instead, changes to
 * language settings should be made using this element's public functions.
 *
 * Use two-way binding syntax to propagate changes from child to host, so that
 * changes made internally to 'languages' propagate to your host element:
 *
 *     <template>
 *       <cr-settings-languages languages="{{languages}}">
 *       </cr-settings-languages>
 *       <div>[[languages.someProperty]]</div>
 *     </template>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-languages
 */

/** @typedef {{spellCheckEnabled: boolean}} */
var LanguageState;

/**
 * @typedef {{language: !chrome.languageSettingsPrivate.Language,
 *            state: !LanguageState}}
 */
var LanguageInfo;

/**
 * supportedLanguages: an array of languages, ordered alphabetically.
 * enabledLanguages: an array of enabled language info and state, ordered by
 *     preference.
 * @typedef {{
 *      supportedLanguages: !Array<!chrome.languageSettingsPrivate.Language>,
 *      enabledLanguages: !Array<!LanguageInfo>
 *  }}
 */
var LanguagesModel;

(function() {
'use strict';

/**
 * This element has a reference to the singleton, exposing the singleton's
 * language model to the host of this element as the 'languages' property.
 */
Polymer({
  is: 'cr-settings-languages',

  properties: {
    /**
     * Singleton element created at startup which provides the languages model.
     * @type {!Element}
     */
    singleton_: {
      type: Object,
      value: document.createElement('cr-settings-languages-singleton'),
    },

    /**
     * A reference to the languages model from the singleton, exposed as a
     * read-only property so hosts can bind to it, but not change it.
     * @type {LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
      readOnly: true,
    },
  },

  ready: function() {
    // Set the 'languages' property to reference the singleton's model.
    this._setLanguages(this.singleton_.languages);
    // Listen for changes to the singleton's languages property, so we know
    // when to notify hosts of changes to (our reference to) the property.
    this.listen(
        this.singleton_, 'languages-changed', 'singletonLanguagesChanged_');
  },

  /**
   * Takes changes reported by the singleton and forwards them to the host,
   * manually sending a change notification for our 'languages' property (since
   * it's the same object as the singleton's property, but isn't bound by
   * Polymer).
   * @private
   */
  singletonLanguagesChanged_: function(e) {
    // Forward the change notification to the host.
    this.fire(e.type, e.detail, {bubbles: false});
  },

  // Forward public methods to the singleton.

  /** @param {string} languageCode */
  setUILanguage: function(languageCode) {
    if (cr.isWindows || cr.isChromeOS)
      this.singleton_.setUILanguage(languageCode);
  },

  /** @param {string} languageCode */
  enableLanguage: function(languageCode) {
    this.singleton_.enableLanguage(languageCode);
  },

  /** @param {string} languageCode */
  disableLanguage: function(languageCode) {
    this.singleton_.disableLanguage(languageCode);
  },

  /**
   * @param {string} languageCode
   * @return {boolean}
   */
  isEnabled: function(languageCode) {
    return this.singleton_.isEnabled(languageCode);
  },

  /**
   * @param {string} languageCode
   * @param {boolean} enable
   */
  toggleSpellCheck: function(languageCode, enable) {
    this.singleton_.toggleSpellCheck(languageCode, enable);
  },
});

var preferredLanguagesPrefName = cr.isChromeOS ?
    'settings.language.preferred_languages' : 'intl.accept_languages';

/**
 * Singleton element created when cr-settings-languages is registered.
 * Generates the languages model on start-up, and updates it whenever Chrome's
 * pref store and other settings change. These updates propagate to each
 * <cr-settings-language> instance so that their 'languages' property updates
 * like any other Polymer property.
 */
Polymer({
  is: 'cr-settings-languages-singleton',

  properties: {
    /**
     * @type {LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
    },

    /**
     * Object containing all preferences.
     */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /**
   * Hash map of languages.supportedLanguages using language codes as keys for
   * fast lookup.
   * @private {!Object<!chrome.languageSettingsPrivate.Language>}
   */
  supportedLanguageMap_: {},

  /**
   * Hash map of languages.enabledLanguages using language codes as keys for
   * fast lookup.
   * @private {!Object<!LanguageInfo>}
   */
  enabledLanguageMap_: {},

  observers: [
    'preferredLanguagesPrefChanged_(prefs.' +
        preferredLanguagesPrefName + '.value)',
    'spellCheckDictionariesPrefChanged_(prefs.spellcheck.dictionaries.value.*)',
  ],

  /** @override */
  created: function() {
    chrome.languageSettingsPrivate.getLanguageList(function(languageList) {
      // Wait until prefs are initialized before creating the model, so we can
      // include information about enabled languages.
      CrSettingsPrefs.initialized.then(function() {
        this.createModel_(languageList);
        this.initialized_ = true;
      }.bind(this));
    }.bind(this));
  },

  /**
   * Constructs the languages model from the given language list.
   * @param {!Array<!chrome.languageSettingsPrivate.Language>}
   *     supportedLanguages
   */
  createModel_: function(supportedLanguages) {
    // Populate the hash map of supported languages.
    for (var i = 0; i < supportedLanguages.length; i++) {
      this.supportedLanguageMap_[supportedLanguages[i].code] =
          supportedLanguages[i];
    }

    // Create a list of enabled language info from the supported languages.
    var enabledLanguages = this.getEnabledLanguages_();
    // Populate the hash map of enabled languages.
    for (var i = 0; i < enabledLanguages.length; i++) {
      var languageInfo = enabledLanguages[i];
      this.enabledLanguageMap_[languageInfo.language.code] = languageInfo;
    }

    // Initialize the Polymer languages model.
    this.languages = {
      supportedLanguages: supportedLanguages,
      enabledLanguages: enabledLanguages,
    };
  },

  /**
   * Returns a list of LanguageInfos for each enabled language in the supported
   * languages list.
   * @return {!Array<!LanguageInfo>}
   * @private
   */
  getEnabledLanguages_: function() {
    assert(CrSettingsPrefs.isInitialized);

    var pref = this.getPref_(preferredLanguagesPrefName);
    var enabledLanguageCodes = pref.value.split(',');
    var enabledLanguages = [];
    var spellCheckMap = this.getSpellCheckMap_();
    for (var i = 0; i < enabledLanguageCodes.length; i++) {
      var code = enabledLanguageCodes[i];
      var language = this.supportedLanguageMap_[code];
      if (!language)
        continue;
      var state = {spellCheckEnabled: !!spellCheckMap[code]};
      enabledLanguages.push({language: language, state: state});
    }
    return enabledLanguages;
  },

  /**
   * Creates a map whose keys are languages enabled for spell check.
   * @return {!Object<boolean>}
   */
  getSpellCheckMap_: function() {
    assert(CrSettingsPrefs.isInitialized);

    var spellCheckCodes = this.getPref_('spellcheck.dictionaries').value;
    var spellCheckMap = {};
    for (var i = 0; i < spellCheckCodes.length; i++)
      spellCheckMap[spellCheckCodes[i]] = true;
    return spellCheckMap;
  },

  /**
   * Updates the list of enabled languages from the preferred languages pref.
   * @private
   * */
  preferredLanguagesPrefChanged_: function() {
    if (!this.initialized_)
      return;

    var enabledLanguages = this.getEnabledLanguages_();
    // Reset the enabled language map. Do this before notifying of the change
    // via languages.enabledLanguages.
    this.enabledLanguageMap_ = {};
    for (var i = 0; i < enabledLanguages.length; i++) {
      var languageInfo = enabledLanguages[i];
      this.enabledLanguageMap_[languageInfo.language.code] = languageInfo;
    }
    this.set('languages.enabledLanguages', enabledLanguages);
  },

  /**
   * Updates the spellCheckEnabled state of each enabled language.
   * @private
   */
  spellCheckDictionariesPrefChanged_: function() {
    if (!this.initialized_)
      return;

    var spellCheckMap = this.getSpellCheckMap_();
    for (var i = 0; i < this.languages.enabledLanguages.length; i++) {
      var languageCode = this.languages.enabledLanguages[i].language.code;
      this.set('languages.enabledLanguages.' + i + '.state.spellCheckEnabled',
               !!spellCheckMap[languageCode]);
    }
  },

  /**
   * Gets the pref at the given key. Asserts if the pref is not found.
   * @param {string} key
   * @return {!chrome.settingsPrivate.PrefObject}
   */
  getPref_: function(key) {
    var pref = /** @type {!chrome.settingsPrivate.PrefObject} */(
        this.get(key, this.prefs));
    assert(typeof pref != 'undefined', 'Pref is missing: ' + key);
    return pref;
  },

  /**
   * Sets the value of the pref at the given key. Asserts if the pref is not
   * found.
   * @param {string} key
   * @param {*} value
   */
  setPrefValue_: function(key, value) {
    this.getPref_(key);
    this.set('prefs.' + key + '.value', value);
  },

  /**
   * Windows and Chrome OS only: Sets the prospective UI language to the chosen
   * language. This dosen't affect the actual UI language until a restart.
   * @param {string} languageCode
   */
  setUILanguage: function(languageCode) {
    chrome.send('setUILanguage', [languageCode]);
  },

  /**
   * Enables the language, making it available for spell check and input.
   * @param {string} languageCode
   */
  enableLanguage: function(languageCode) {
    if (!CrSettingsPrefs.isInitialized)
      return;

    var languageCodes = this.getPref_(preferredLanguagesPrefName).value;
    var index = languageCodes.split(',').indexOf(languageCode);
    if (index > -1)
      return;
    this.setPrefValue_(preferredLanguagesPrefName,
                       languageCodes + ',' + languageCode);
  },

  /**
   * Disables the language.
   * @param {string} languageCode
   */
  disableLanguage: function(languageCode) {
    if (!CrSettingsPrefs.isInitialized)
      return;

    // Cannot disable the UI language.
    var appLocale = this.getPref_('intl.app_locale').value ||
                    navigator.language;
    assert(languageCode != appLocale);

    // Cannot disable the only enabled language.
    var pref = this.getPref_(preferredLanguagesPrefName);
    var languageCodes = pref.value.split(',');
    assert(languageCodes.length > 1);

    // Remove the language from spell check.
    this.arrayDelete('prefs.spellcheck.dictionaries.value', languageCode);

    var languageIndex = languageCodes.indexOf(languageCode);
    if (languageIndex == -1)
      return;
    languageCodes.splice(languageIndex, 1);
    this.setPrefValue_(preferredLanguagesPrefName, languageCodes.join(','));
  },

  /**
   * @param {string} languageCode
   * @return {boolean} True if the language is enabled.
   */
  isEnabled: function(languageCode) {
    return !!this.enabledLanguageMap_[languageCode];
  },

  /**
   * Enables or disables spell check for the given language.
   * @param {string} languageCode
   * @param {boolean} enable
   */
  toggleSpellCheck: function(languageCode, enable) {
    if (!this.initialized_)
      return;

    if (enable) {
      var spellCheckPref = this.getPref_('spellcheck.dictionaries');
      if (spellCheckPref.value.indexOf(languageCode) == -1)
        this.push('prefs.spellcheck.dictionaries.value', languageCode);
    } else {
      this.arrayDelete('prefs.spellcheck.dictionaries.value', languageCode);
    }
  },
});
})();
