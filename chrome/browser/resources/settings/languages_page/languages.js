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
     * @type {!LanguagesModel|undefined}
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

    /** @type {!LanguageSettingsPrivate} */
    languageSettingsPrivate: Object,
  },

  /**
   * Hash map of supported languages by language codes for fast lookup.
   * @private {!Map<string, !chrome.languageSettingsPrivate.Language>}
   */
  supportedLanguageMap_: new Map(),

  /**
   * Hash set of enabled language codes for membership testing.
   * @private {!Set<string>}
   */
  enabledLanguageSet_: new Set(),

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
    'prospectiveUILanguageChanged_(' +
        'prefs.intl.app_locale.value,' +
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
    var enabledLanguageStates =
        this.getEnabledLanguageStates_(this.languages.translateTarget);

    // Recreate the enabled language set before updating languages.enabled.
    this.enabledLanguageSet_.clear();
    for (var languageState of enabledLanguageStates)
      this.enabledLanguageSet_.add(languageState.language.code);

    this.set('languages.enabled', enabledLanguageStates);
    this.updateRemovableLanguages_();
  },

  /**
   * Updates the spellCheckEnabled state of each enabled language.
   * @private
   */
  spellCheckDictionariesPrefChanged_: function() {
    var spellCheckSet = this.makeSetFromArray_(/** @type {!Array<string>} */(
        this.getPref('spellcheck.dictionaries').value));
    for (var i = 0; i < this.languages.enabled.length; i++) {
      var languageState = this.languages.enabled[i];
      this.set('languages.enabled.' + i + '.spellCheckEnabled',
               !!spellCheckSet.has(languageState.language.code));
    }
  },

  /** @private */
  translateLanguagesPrefChanged_: function() {
    var translateBlockedPref = this.getPref('translate_blocked_languages');
    var translateBlockedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */(translateBlockedPref.value));

    for (var i = 0; i < this.languages.enabled.length; i++) {
      var translateCode = this.convertLanguageCodeForTranslate(
          this.languages.enabled[i].language.code);
      this.set(
          'languages.enabled.' + i + '.translateEnabled',
          !translateBlockedSet.has(translateCode));
    }
  },

  /** @private */
  prospectiveUILanguageChanged_: function() {
    this.updateRemovableLanguages_();
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
    for (var language of supportedLanguages) {
      language.supportsUI = !!language.supportsUI;
      language.supportsTranslate = !!language.supportsTranslate;
      language.supportsSpellcheck = !!language.supportsSpellcheck;
      this.supportedLanguageMap_.set(language.code, language);
    }

    // Create a list of enabled languages from the supported languages.
    var enabledLanguageStates = this.getEnabledLanguageStates_(translateTarget);
    // Populate the hash set of enabled languages.
    for (var languageState of enabledLanguageStates)
      this.enabledLanguageSet_.add(languageState.language.code);

    // Initialize the Polymer languages model.
    this.languages = /** @type {!LanguagesModel} */({
      supported: supportedLanguages,
      enabled: enabledLanguageStates,
      translateTarget: translateTarget,
    });
  },

  /**
   * Returns a list of LanguageStates for each enabled language in the supported
   * languages list.
   * @param {string} translateTarget Language code of the default translate
   *     target language.
   * @return {!Array<!LanguageState>}
   * @private
   */
  getEnabledLanguageStates_: function(translateTarget) {
    assert(CrSettingsPrefs.isInitialized);

    var pref = this.getPref(preferredLanguagesPrefName);
    var enabledLanguageCodes = pref.value.split(',');
    var spellCheckPref = this.getPref('spellcheck.dictionaries');
    var spellCheckSet = this.makeSetFromArray_(/** @type {!Array<string>} */(
        spellCheckPref.value));

    var translateBlockedPref = this.getPref('translate_blocked_languages');
    var translateBlockedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */(translateBlockedPref.value));

    var enabledLanguageStates = [];
    for (var i = 0; i < enabledLanguageCodes.length; i++) {
      var code = enabledLanguageCodes[i];
      var language = this.supportedLanguageMap_.get(code);
      // Skip unsupported languages.
      if (!language)
        continue;
      var languageState = /** @type {LanguageState} */({});
      languageState.language = language;
      languageState.spellCheckEnabled = !!spellCheckSet.has(code);
      // Translate is considered disabled if this language maps to any translate
      // language that is blocked.
      var translateCode = this.convertLanguageCodeForTranslate(code);
      languageState.translateEnabled = !!language.supportsTranslate &&
          !translateBlockedSet.has(translateCode) &&
          translateCode != translateTarget;
      enabledLanguageStates.push(languageState);
    }
    return enabledLanguageStates;
  },

  /**
   * Updates the |removable| property of the enabled language states based
   * on what other languages are enabled.
   * @private
   */
  updateRemovableLanguages_: function() {
    assert(this.languages);
    for (var i = 0; i < this.languages.enabled.length; i++) {
      var languageState = this.languages.enabled[i];
      this.set('languages.enabled.' + i + '.removable',
          this.canDisableLanguage(languageState.language.code));
    }
  },

  /**
   * Creates a Set from the elements of the arary.
   * @param {!Array<T>} list
   * @return {!Set<T>}
   * @template T
   * @private
   */
  makeSetFromArray_: function(list) {
    var set = new Set();
    for (var item of list)
      set.add(item);
    return set;
  },

  // LanguageHelper implementation.
  // TODO(michaelpg): replace duplicate docs with @override once b/24294625
  // is fixed.

  /** @return {!Promise} */
  whenReady: function() {
    return this.resolver_.promise;
  },

  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   * @param {string} languageCode
   */
  setUILanguage: function(languageCode) {
    assert(cr.isChromeOS || cr.isWindows);
    chrome.send('setUILanguage', [languageCode]);
  },

  /** Resets the prospective UI language back to the actual UI language. */
  resetUILanguage: function() {
    assert(cr.isChromeOS || cr.isWindows);
    chrome.send('setUILanguage', [navigator.language]);
  },

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
    return this.enabledLanguageSet_.has(languageCode);
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
    if (this.languages.enabled.length == 1)
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
    return this.supportedLanguageMap_.get(languageCode);
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
