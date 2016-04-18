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
 * language settings should be made using the LanguageHelperImpl singleton.
 *
 * Use upward binding syntax to propagate changes from child to host, so that
 * changes made internally to 'languages' propagate to your host element:
 *
 *     <template>
 *       <settings-languages languages="{{languages}}">
 *       </settings-languages>
 *       <div>[[languages.someProperty]]</div>
 *     </template>
 */

var SettingsLanguagesSingletonElement;

cr.exportPath('languageSettings');

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

var preferredLanguagesPrefName = cr.isChromeOS ?
    'settings.language.preferred_languages' : 'intl.accept_languages';

/**
 * Singleton element that generates the languages model on start-up and
 * updates it whenever Chrome's pref store and other settings change. These
 * updates propagate to each <settings-language> instance so that their
 * 'languages' property updates like any other Polymer property.
 * @implements {LanguageHelper}
 */
SettingsLanguagesSingletonElement = Polymer({
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

    /**
     * PromiseResolver to be resolved when the singleton has been initialized.
     * @private {!PromiseResolver}
     */
    resolver_: {
      type: Object,
      value: function() {
        return new PromiseResolver();
      },
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
    'preferredLanguagesPrefChanged_(' +
        'prefs.' + preferredLanguagesPrefName + '.value, ' +
        'languages)',
    'spellCheckDictionariesPrefChanged_(' +
        'prefs.spellcheck.dictionaries.value.*, ' +
        'languages)',
    'translateLanguagesPrefChanged_(' +
        'prefs.translate_blocked_languages.value.*,' +
        'languages)',
  ],

  /** @override */
  created: function() {
    this.languageSettingsPrivate =
        languageSettings.languageSettingsPrivateApiForTest ||
        /** @type {!LanguageSettingsPrivate} */(chrome.languageSettingsPrivate);

    var promises = [
      // Wait until prefs are initialized before creating the model, so we can
      // include information about enabled languages.
      CrSettingsPrefs.initialized,

      // Get the language list.
      new Promise(function(resolve) {
        this.languageSettingsPrivate.getLanguageList(resolve);
      }.bind(this)),

      // Get the translate target language.
      new Promise(function(resolve) {
        this.languageSettingsPrivate.getTranslateTargetLanguage(resolve);
      }.bind(this)),
    ];

    Promise.all(promises).then(function(results) {
       this.createModel_(results[1], results[2]);
       this.resolver_.resolve();
    }.bind(this));
  },

  /**
   * Updates the list of enabled languages from the preferred languages pref.
   * @private
   */
  preferredLanguagesPrefChanged_: function() {
    var enabledLanguages =
        this.getEnabledLanguages_(this.languages.translateTarget);

    // Reset the enabled language map before updating
    // languages.enabledLanguages.
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
    var spellCheckMap = this.makeMapFromArray_(/** @type {!Array<string>} */(
        this.getPref('spellcheck.dictionaries').value));
    for (var i = 0; i < this.languages.enabledLanguages.length; i++) {
      var languageCode = this.languages.enabledLanguages[i].language.code;
      this.set('languages.enabledLanguages.' + i + '.state.spellCheckEnabled',
               !!spellCheckMap[languageCode]);
    }
  },

  /** @private */
  translateLanguagesPrefChanged_: function() {
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
   * Constructs the languages model.
   * @param {!Array<!chrome.languageSettingsPrivate.Language>}
   *     supportedLanguages
   * @param {string} translateTarget Language code of the default translate
   *     target language.
   * @private
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
      // Skip unsupported languages.
      if (!language)
        continue;
      var state = /** @type {LanguageState} */({});
      state.spellCheckEnabled = !!spellCheckMap[code];
      // Translate is considered disabled if this language maps to any translate
      // language that is blocked.
      var translateCode = this.convertLanguageCodeForTranslate(code);
      state.translateEnabled = !!language.supportsTranslate &&
          !translateBlockedMap[translateCode] &&
          translateCode != translateTarget;
      enabledLanguages.push(/** @type {LanguageInfo} */(
          {language: language, state: state}));
    }
    return enabledLanguages;
  },

  /**
   * Creates an object whose keys are the elements of the list.
   * @param {!Array<string>} list
   * @return {!Object<boolean>}
   * @private
   */
  makeMapFromArray_: function(list) {
    var map = {};
    for (var i = 0; i < list.length; i++)
      map[list[i]] = true;
    return map;
  },

  // LanguageHelper implementation.
  // TODO(michaelpg): replace duplicate docs with @override once b/24294625
  // is fixed.

  /** @return {!Promise} */
  whenReady: function() {
    return this.resolver_.promise;
  },

<if expr="chromeos or is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   * @param {string} languageCode
   */
  setUILanguage: function(languageCode) {
    chrome.send('setUILanguage', [languageCode]);
  },

  /** Resets the prospective UI language back to the actual UI language. */
  resetUILanguage: function() {
    chrome.send('setUILanguage', [navigator.language]);
  },
</if>

  /**
   * Returns the "prospective" UI language, i.e. the one to be used on next
   * restart. If the pref is not set, the current UI language is also the
   * "prospective" language.
   * @return {string} Language code of the prospective UI language.
   */
  getProspectiveUILanguage: function() {
    return /** @type {string} */(this.getPref('intl.app_locale').value) ||
        navigator.language;
  },

  /**
   * @param {string} languageCode
   * @return {boolean} True if the language is enabled.
   */
  isLanguageEnabled: function(languageCode) {
    return !!this.enabledLanguageMap_[languageCode];
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
    this.languageSettingsPrivate.setLanguageList(languageCodes);
    this.disableTranslateLanguage(languageCode);
  },

  /**
   * Disables the language.
   * @param {string} languageCode
   */
  disableLanguage: function(languageCode) {
    if (!CrSettingsPrefs.isInitialized)
      return;

    assert(this.canDisableLanguage(languageCode));

    // Remove the language from spell check.
    this.deletePrefListItem('spellcheck.dictionaries', languageCode);

    // Remove the language from preferred languages.
    var languageCodes =
        this.getPref(preferredLanguagesPrefName).value.split(',');
    var languageIndex = languageCodes.indexOf(languageCode);
    if (languageIndex == -1)
      return;
    languageCodes.splice(languageIndex, 1);
    this.languageSettingsPrivate.setLanguageList(languageCodes);
    this.enableTranslateLanguage(languageCode);
  },

  /**
   * @param {string} languageCode Language code for an enabled language.
   * @return {boolean}
   */
  canDisableLanguage: function(languageCode) {
    // Cannot disable the prospective UI language.
    if ((cr.isChromeOS || cr.isWindows) &&
        languageCode == this.getProspectiveUILanguage()) {
      return false;
    }

    // Cannot disable the only enabled language.
    if (this.languages.enabledLanguages.length == 1)
      return false;

    return true;
  },

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   * @param {string} languageCode
   */
  enableTranslateLanguage: function(languageCode) {
    languageCode = this.convertLanguageCodeForTranslate(languageCode);
    this.deletePrefListItem('translate_blocked_languages', languageCode);
  },

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   * @param {string} languageCode
   */
  disableTranslateLanguage: function(languageCode) {
    this.appendPrefListItem('translate_blocked_languages',
        this.convertLanguageCodeForTranslate(languageCode));
  },

  /**
   * Enables or disables spell check for the given language.
   * @param {string} languageCode
   * @param {boolean} enable
   */
  toggleSpellCheck: function(languageCode, enable) {
    if (!this.languages)
      return;

    if (enable) {
      var spellCheckPref = this.getPref('spellcheck.dictionaries');
      this.appendPrefListItem('spellcheck.dictionaries', languageCode);
    } else {
      this.deletePrefListItem('spellcheck.dictionaries', languageCode);
    }
  },

  /**
   * Converts the language code for translate. There are some differences
   * between the language set the Translate server uses and that for
   * Accept-Language.
   * @param {string} languageCode
   * @return {string} The converted language code.
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

  /**
   * @param {string} languageCode
   * @return {!chrome.languageSettingsPrivate.Language|undefined}
   */
  getLanguage: function(languageCode) {
    return this.supportedLanguageMap_[languageCode];
  },
});
})();

/**
 * A reference to the singleton under the guise of a LanguageHelper
 * implementation. This provides a limited API but implies the singleton
 * should not be used directly for data binding.
 */
var LanguageHelperImpl = SettingsLanguagesSingletonElement;
cr.addSingletonGetter(LanguageHelperImpl);

/**
 * This element has a reference to the singleton, exposing the singleton's
 * |languages| model to the host of this element.
 */
Polymer({
  is: 'settings-languages',

  properties: {
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
    var singleton = /** @type {!SettingsLanguagesSingletonElement} */
        (LanguageHelperImpl.getInstance());
    singleton.whenReady().then(function() {
      // Set the 'languages' property to reference the singleton's model.
      this._setLanguages(singleton.languages);
      // Listen for changes to the singleton's languages property, so we know
      // when to notify hosts of changes to (our reference to) the property.
      this.listen(singleton, 'languages-changed', 'singletonLanguagesChanged_');
    }.bind(this));
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
});
