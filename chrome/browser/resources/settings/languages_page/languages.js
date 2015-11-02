// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages' provides convenient access to
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
 *       <settings-languages languages="{{languages}}">
 *       </settings-languages>
 *       <div>[[languages.someProperty]]</div>
 *     </template>
 *
 * @group Chrome Settings Elements
 * @element settings-languages
 */

/** @typedef {{spellCheckEnabled: boolean, translateEnabled: boolean}} */
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
 *      enabledLanguages: !Array<!LanguageInfo>,
 *      translateTarget: string
 *  }}
 */
var LanguagesModel;

(function() {
'use strict';

// Translate server treats some language codes the same.
// See also: components/translate/core/common/translate_util.cc.
var kLanguageCodeToTranslateCode = {
  'nb': 'no',
  'fil': 'tl',
  'zh-HK': 'zh-TW',
  'zh-MO': 'zh-TW',
  'zh-SG': 'zh-CN',
};

// Some ISO 639 language codes have been renamed, e.g. "he" to "iw", but
// Translate still uses the old versions. TODO(michaelpg): Chrome does too.
// Follow up with Translate owners to understand the right thing to do.
var kTranslateLanguageSynonyms = {
  'he': 'iw',
  'jv': 'jw',
};

/**
 * This element has a reference to the singleton, exposing the singleton's
 * language model to the host of this element as the 'languages' property.
 */
Polymer({
  is: 'settings-languages',

  properties: {
    /**
     * Singleton element created at startup which provides the languages model.
     * @type {!Element}
     */
    singleton_: {
      type: Object,
      value: document.createElement('settings-languages-singleton'),
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

  resetUILanguage: function() {
    if (cr.isWindows || cr.isChromeOS)
      this.singleton_.resetUILanguage();
  },

  /** @return {string} */
  getProspectiveUILanguage: function() {
    return this.singleton_.getProspectiveUILanguage();
  },

  /** @param {string} languageCode */
  enableLanguage: function(languageCode) {
    this.singleton_.enableLanguage(languageCode);
  },

  /** @param {string} languageCode */
  disableLanguage: function(languageCode) {
    this.singleton_.disableLanguage(languageCode);
  },

  /** @param {string} languageCode */
  enableTranslateLanguage: function(languageCode) {
    this.singleton_.enableTranslateLanguage(languageCode);
  },

  /** @param {string} languageCode */
  disableTranslateLanguage: function(languageCode) {
    this.singleton_.disableTranslateLanguage(languageCode);
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

  /**
   * @param {string} languageCode
   * @return {string}
   */
  convertLanguageCodeForTranslate: function(languageCode) {
    return this.singleton_.convertLanguageCodeForTranslate(languageCode);
  },
});

var preferredLanguagesPrefName = cr.isChromeOS ?
    'settings.language.preferred_languages' : 'intl.accept_languages';

/**
 * Singleton element created when settings-languages is registered.
 * Generates the languages model on start-up, and updates it whenever Chrome's
 * pref store and other settings change. These updates propagate to each
 * <settings-language> instance so that their 'languages' property updates
 * like any other Polymer property.
 */
Polymer({
  is: 'settings-languages-singleton',

  behaviors: [PrefsBehavior],

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
    'translateLanguagesPrefChanged_(prefs.translate_blocked_languages.value.*)',
  ],

  /** @override */
  created: function() {
    var languageList;
    var translateTarget;

    // Request language information to populate the model.
    Promise.all([
      // Wait until prefs are initialized before creating the model, so we can
      // include information about enabled languages.
      CrSettingsPrefs.initialized,

      // Get the language list.
      new Promise(function(resolve) {
        chrome.languageSettingsPrivate.getLanguageList(function(list) {
          languageList = list;
          resolve();
        });
      }),

      // Get the translate target language.
      new Promise(function(resolve) {
        chrome.languageSettingsPrivate.getTranslateTargetLanguage(
            function(targetLanguageCode) {
              translateTarget = targetLanguageCode;
              resolve();
            });
      }),
    ]).then(function() {
      this.createModel_(languageList, translateTarget);
      this.initialized_ = true;
    }.bind(this));
  },

  /**
   * Constructs the languages model.
   * @param {!Array<!chrome.languageSettingsPrivate.Language>}
   *     supportedLanguages
   * @param {string} translateTarget Language code of the default translate
   *     target language.
   */
  createModel_: function(supportedLanguages, translateTarget) {
    // Populate the hash map of supported languages.
    for (var i = 0; i < supportedLanguages.length; i++) {
      var language = supportedLanguages[i];
      language.supportsUI = !!language.supportsUI;
      language.supportsTranslate = !!language.supportsTranslate;
      language.supportsSpellcheck = !!language.supportsSpellcheck;
      this.supportedLanguageMap_[language.code] = language;
    }

    // Create a list of enabled language info from the supported languages.
    var enabledLanguages = this.getEnabledLanguages_(translateTarget);
    // Populate the hash map of enabled languages.
    for (var i = 0; i < enabledLanguages.length; i++) {
      var languageInfo = enabledLanguages[i];
      this.enabledLanguageMap_[languageInfo.language.code] = languageInfo;
    }

    // Initialize the Polymer languages model.
    this.languages = /** @type {!LanguagesModel} */({
      supportedLanguages: supportedLanguages,
      enabledLanguages: enabledLanguages,
      translateTarget: translateTarget,
    });
  },

  /**
   * Returns a list of LanguageInfos for each enabled language in the supported
   * languages list.
   * @param {string} translateTarget Language code of the default translate
   *     target language.
   * @return {!Array<!LanguageInfo>}
   * @private
   */
  getEnabledLanguages_: function(translateTarget) {
    assert(CrSettingsPrefs.isInitialized);

    var pref = this.getPref(preferredLanguagesPrefName);
    var enabledLanguageCodes = pref.value.split(',');
    var enabledLanguages = /** @type {!Array<!LanguageInfo>} */ [];

    var spellCheckPref = this.getPref('spellcheck.dictionaries');
    var spellCheckMap = this.makeMapFromArray_(/** @type {!Array<string>} */(
        spellCheckPref.value));

    var translateBlockedPref = this.getPref('translate_blocked_languages');
    var translateBlockedMap = this.makeMapFromArray_(
        /** @type {!Array<string>} */(translateBlockedPref.value));

    for (var i = 0; i < enabledLanguageCodes.length; i++) {
      var code = enabledLanguageCodes[i];
      var language = this.supportedLanguageMap_[code];
      if (!language)
        continue;
      var state = {};
      state.spellCheckEnabled = !!spellCheckMap[code];
      // Translate is considered disabled if this language maps to any translate
      // language that is blocked.
      var translateCode = this.convertLanguageCodeForTranslate(code);
      state.translateEnabled = language.supportsTranslate &&
          !translateBlockedMap[translateCode] &&
          translateCode != translateTarget;
      enabledLanguages.push({language: language, state: state});
    }
    return enabledLanguages;
  },

  /**
   * Creates an object whose keys are the elements of the list.
   * @param {!Array<string>} list
   * @return {!Object<boolean>}
   */
  makeMapFromArray_: function(list) {
    var map = {};
    for (var i = 0; i < list.length; i++)
      map[list[i]] = true;
    return map;
  },

  /**
   * Updates the list of enabled languages from the preferred languages pref.
   * @private
   * */
  preferredLanguagesPrefChanged_: function() {
    if (!this.initialized_)
      return;

    var enabledLanguages =
        this.getEnabledLanguages_(this.languages.translateTarget);
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

    var spellCheckMap = this.makeMapFromArray_(/** @type {!Array<string>} */(
        this.getPref('spellcheck.dictionaries').value));
    for (var i = 0; i < this.languages.enabledLanguages.length; i++) {
      var languageCode = this.languages.enabledLanguages[i].language.code;
      this.set('languages.enabledLanguages.' + i + '.state.spellCheckEnabled',
               !!spellCheckMap[languageCode]);
    }
  },

  translateLanguagesPrefChanged_: function() {
    if (!this.initialized_)
      return;

    var translateBlockedPref = this.getPref('translate_blocked_languages');
    var translateBlockedMap = this.makeMapFromArray_(
        /** @type {!Array<string>} */(translateBlockedPref.value));

    for (var i = 0; i < this.languages.enabledLanguages.length; i++) {
      var translateCode = this.convertLanguageCodeForTranslate(
          this.languages.enabledLanguages[i].language.code);
      this.set(
          'languages.enabledLanguages.' + i + '.state.translateEnabled',
          !translateBlockedMap[translateCode]);
    }
  },

  /**
   * Deletes the given item from the pref at the given key if the item is found.
   * Asserts if the pref itself is not found or is not an Array type.
   * @param {string} key
   * @param {*} item
   */
  deletePrefItem_: function(key, item) {
    assert(this.getPref(key).type == chrome.settingsPrivate.PrefType.LIST);
    this.arrayDelete('prefs.' + key + '.value', item);
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
   * Windows and Chrome OS only: Resets the prospective UI language back to the
   * actual UI language.
   */
  resetUILanguage: function() {
    chrome.send('setUILanguage', [navigator.language]);
  },

  /**
   * Returns the "prospective" UI language, i.e. the one to be used on next
   * restart. If the pref is not set, the current UI language is also the
   * "prospective" language.
   * @return {string} Language code of the prospective UI language.
   * @private
   */
  getProspectiveUILanguage: function() {
    return /** @type {string} */(this.getPref('intl.app_locale').value) ||
        navigator.language;
  },

  /**
   * Enables the language, making it available for spell check and input.
   * @param {string} languageCode
   */
  enableLanguage: function(languageCode) {
    if (!CrSettingsPrefs.isInitialized)
      return;

    var languageCodes =
        this.getPref(preferredLanguagesPrefName).value.split(',');
    if (languageCodes.indexOf(languageCode) > -1)
      return;
    languageCodes.push(languageCode);
    chrome.languageSettingsPrivate.setLanguageList(languageCodes);
    this.disableTranslateLanguage(languageCode);
  },

  /**
   * Disables the language.
   * @param {string} languageCode
   */
  disableLanguage: function(languageCode) {
    if (!CrSettingsPrefs.isInitialized)
      return;

    // Cannot disable the UI language.
    assert(languageCode != this.getProspectiveUILanguage());

    // Cannot disable the only enabled language.
    var languageCodes =
        this.getPref(preferredLanguagesPrefName).value.split(',');
    assert(languageCodes.length > 1);

    // Remove the language from spell check.
    this.deletePrefItem_('spellcheck.dictionaries', languageCode);

    var languageIndex = languageCodes.indexOf(languageCode);
    if (languageIndex == -1)
      return;
    languageCodes.splice(languageIndex, 1);
    chrome.languageSettingsPrivate.setLanguageList(languageCodes);
    this.enableTranslateLanguage(languageCode);
  },

  /**
   * @param {string} languageCode
   * @return {boolean} True if the language is enabled.
   */
  isEnabled: function(languageCode) {
    return !!this.enabledLanguageMap_[languageCode];
  },

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   * @param {string} languageCode
   */
  enableTranslateLanguage: function(languageCode) {
    languageCode = this.convertLanguageCodeForTranslate(languageCode);
    this.arrayDelete('prefs.translate_blocked_languages.value', languageCode);
  },

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   * @param {string} languageCode
   */
  disableTranslateLanguage: function(languageCode) {
    languageCode = this.convertLanguageCodeForTranslate(languageCode);
    if (this.getPref('translate_blocked_languages').value
            .indexOf(languageCode) == -1) {
      this.push('prefs.translate_blocked_languages.value', languageCode);
    }
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
      var spellCheckPref = this.getPref('spellcheck.dictionaries');
      if (spellCheckPref.value.indexOf(languageCode) == -1)
        this.push('prefs.spellcheck.dictionaries.value', languageCode);
    } else {
      this.arrayDelete('prefs.spellcheck.dictionaries.value', languageCode);
    }
  },

  /**
   * Converts the language code for translate. There are some differences
   * between the language set the Translate server uses and that for
   * Accept-Language.
   * @param {string} languageCode
   * @return {string} The converted language code.
   * @private
   */
  convertLanguageCodeForTranslate: function(languageCode) {
    if (languageCode in kLanguageCodeToTranslateCode)
      return kLanguageCodeToTranslateCode[languageCode];

    var main = languageCode.split('-')[0];
    if (main == 'zh') {
      // In Translate, general Chinese is not used, and the sub code is
      // necessary as a language code for the Translate server.
      return languageCode;
    }
    if (main in kTranslateLanguageSynonyms)
      return kTranslateLanguageSynonyms[main];

    return main;
  },
});
})();
