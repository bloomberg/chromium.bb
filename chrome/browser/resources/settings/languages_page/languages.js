// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages' handles Chrome's language and input
 * method settings. The 'languages' property, which reflects the current
 * language settings, must not be changed directly. Instead, changes to
 * language settings should be made using the LanguageHelper APIs provided by
 * this class via languageHelper.
 */

(function() {
'use strict';

cr.exportPath('settings');

// Translate server treats some language codes the same.
// See also: components/translate/core/common/translate_util.cc.
var kLanguageCodeToTranslateCode = {
  'nb': 'no',
  'fil': 'tl',
  'zh-HK': 'zh-TW',
  'zh-MO': 'zh-TW',
  'zh-SG': 'zh-CN',
  'zh': 'zh-CH',
};

// Some ISO 639 language codes have been renamed, e.g. "he" to "iw", but
// Translate still uses the old versions. TODO(michaelpg): Chrome does too.
// Follow up with Translate owners to understand the right thing to do.
var kTranslateLanguageSynonyms = {
  'he': 'iw',
  'jv': 'jw',
};

var preferredLanguagesPrefName = cr.isChromeOS ?
    'settings.language.preferred_languages' :
    'intl.accept_languages';

/**
 * Singleton element that generates the languages model on start-up and
 * updates it whenever Chrome's pref store and other settings change.
 * @implements {LanguageHelper}
 */
Polymer({
  is: 'settings-languages',

  behaviors: [PrefsBehavior],

  properties: {
    /**
     * @type {!LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
      readOnly: true,
    },

    /**
     * This element, as a LanguageHelper instance for API usage.
     * @type {!LanguageHelper}
     */
    languageHelper: {
      type: Object,
      notify: true,
      readOnly: true,
      value: function() {
        return /** @type {!LanguageHelper} */ (this);
      },
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

    /**
     * Hash map of supported languages by language codes for fast lookup.
     * @private {!Map<string, !chrome.languageSettingsPrivate.Language>}
     */
    supportedLanguageMap_: {
      type: Object,
      value: function() {
        return new Map();
      },
    },

    /**
     * Hash set of enabled language codes for membership testing.
     * @private {!Set<string>}
     */
    enabledLanguageSet_: {
      type: Object,
      value: function() {
        return new Set();
      },
    },

    /**
     * Hash map of supported input methods by ID for fast lookup.
     * @private {!Map<string, chrome.languageSettingsPrivate.InputMethod>}
     */
    supportedInputMethodMap_: {
      type: Object,
      value: function() {
        return new Map();
      },
    },

    /**
     * Hash map of input methods supported for each language.
     * @type {!Map<string,
     *             !Array<!chrome.languageSettingsPrivate.InputMethod>>}
     * @private
     */
    languageInputMethods_: {
      type: Object,
      value: function() {
        return new Map();
      },
    },

    /** @private Prospective UI language when the page was loaded. */
    originalProspectiveUILanguage_: String,
  },

  observers: [
    // All observers wait for the model to be populated by including the
    // |languages| property.
    'prospectiveUILanguageChanged_(prefs.intl.app_locale.value, languages)',
    'preferredLanguagesPrefChanged_(' +
        'prefs.' + preferredLanguagesPrefName + '.value, languages)',
    'spellCheckDictionariesPrefChanged_(' +
        'prefs.spellcheck.dictionaries.value.*, languages)',
    'translateLanguagesPrefChanged_(' +
        'prefs.translate_blocked_languages.value.*, languages)',
    'updateRemovableLanguages_(' +
        'prefs.intl.app_locale.value, languages.enabled)',
    // Observe Chrome OS prefs (ignored for non-Chrome OS).
    'updateRemovableLanguages_(' +
        'prefs.settings.language.preload_engines.value, ' +
        'prefs.settings.language.enabled_extension_imes.value, ' +
        'languages)',
  ],

  /** @private {?Function} */
  boundOnInputMethodChanged_: null,

  /** @private {?settings.LanguagesBrowserProxy} */
  browserProxy_: null,

  /** @private {?LanguageSettingsPrivate} */
  languageSettingsPrivate_: null,

  // <if expr="chromeos">
  /** @private {?InputMethodPrivate} */
  inputMethodPrivate_: null,
  // </if>

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.LanguagesBrowserProxyImpl.getInstance();
    this.languageSettingsPrivate_ =
        this.browserProxy_.getLanguageSettingsPrivate();
    // <if expr="chromeos">
    this.inputMethodPrivate_ = this.browserProxy_.getInputMethodPrivate();
    // </if>

    var promises = [];

    // Wait until prefs are initialized before creating the model, so we can
    // include information about enabled languages.
    promises[0] = CrSettingsPrefs.initialized;

    // Get the language list.
    promises[1] = new Promise(resolve => {
      this.languageSettingsPrivate_.getLanguageList(resolve);
    });

    // Get the translate target language.
    promises[2] = new Promise(resolve => {
      this.languageSettingsPrivate_.getTranslateTargetLanguage(resolve);
    });

    if (cr.isChromeOS) {
      promises[3] = new Promise(resolve => {
        this.languageSettingsPrivate_.getInputMethodLists(function(lists) {
          resolve(lists.componentExtensionImes.concat(
              lists.thirdPartyExtensionImes));
        });
      });

      promises[4] = new Promise(resolve => {
        this.inputMethodPrivate_.getCurrentInputMethod(resolve);
      });
    }

    if (cr.isWindows || cr.isChromeOS) {
      // Fetch the starting UI language, which affects which actions should be
      // enabled.
      promises.push(this.browserProxy_.getProspectiveUILanguage().then(
          prospectiveUILanguage => {
            this.originalProspectiveUILanguage_ =
                prospectiveUILanguage || window.navigator.language;
          }));
    }

    Promise.all(promises).then(results => {
      if (!this.isConnected) {
        // Return early if this element was detached from the DOM before this
        // async callback executes (can happen during testing).
        return;
      }

      // TODO(dpapad): Cleanup this code. It uses results[3] and results[4]
      // which only exist for ChromeOS.
      this.createModel_(results[1], results[2], results[3], results[4]);
      this.resolver_.resolve();
    });

    if (cr.isChromeOS) {
      this.boundOnInputMethodChanged_ = this.onInputMethodChanged_.bind(this);
      this.inputMethodPrivate_.onChanged.addListener(
          assert(this.boundOnInputMethodChanged_));
    }
  },

  /** @override */
  detached: function() {
    if (cr.isChromeOS) {
      this.inputMethodPrivate_.onChanged.removeListener(
          assert(this.boundOnInputMethodChanged_));
      this.boundOnInputMethodChanged_ = null;
    }
  },

  /**
   * Updates the prospective UI language based on the new pref value.
   * @param {string} prospectiveUILanguage
   * @private
   */
  prospectiveUILanguageChanged_: function(prospectiveUILanguage) {
    this.set(
        'languages.prospectiveUILanguage',
        prospectiveUILanguage || this.originalProspectiveUILanguage_);
  },

  /**
   * Updates the list of enabled languages from the preferred languages pref.
   * @private
   */
  preferredLanguagesPrefChanged_: function() {
    var enabledLanguageStates = this.getEnabledLanguageStates_(
        this.languages.translateTarget, this.languages.prospectiveUILanguage);

    // Recreate the enabled language set before updating languages.enabled.
    this.enabledLanguageSet_.clear();
    for (var i = 0; i < enabledLanguageStates.length; i++)
      this.enabledLanguageSet_.add(enabledLanguageStates[i].language.code);

    this.set('languages.enabled', enabledLanguageStates);
  },

  /**
   * Updates the spellCheckEnabled state of each enabled language.
   * @private
   */
  spellCheckDictionariesPrefChanged_: function() {
    var spellCheckSet = this.makeSetFromArray_(/** @type {!Array<string>} */ (
        this.getPref('spellcheck.dictionaries').value));
    for (var i = 0; i < this.languages.enabled.length; i++) {
      var languageState = this.languages.enabled[i];
      this.set(
          'languages.enabled.' + i + '.spellCheckEnabled',
          !!spellCheckSet.has(languageState.language.code));
    }
  },

  /** @private */
  translateLanguagesPrefChanged_: function() {
    var translateBlockedPref = this.getPref('translate_blocked_languages');
    var translateBlockedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */ (translateBlockedPref.value));

    for (var i = 0; i < this.languages.enabled.length; i++) {
      if (this.languages.enabled[i].language.code ==
          this.languages.prospectiveUILanguage) {
        continue;
      }
      // This conversion primarily strips away the region part.
      // For example "fr-CA" --> "fr".
      var translateCode = this.convertLanguageCodeForTranslate(
          this.languages.enabled[i].language.code);
      this.set(
          'languages.enabled.' + i + '.translateEnabled',
          !translateBlockedSet.has(translateCode));
    }
  },

  /**
   * Constructs the languages model.
   * @param {!Array<!chrome.languageSettingsPrivate.Language>}
   *     supportedLanguages
   * @param {string} translateTarget Language code of the default translate
   *     target language.
   * @param {!Array<!chrome.languageSettingsPrivate.InputMethod>|undefined}
   *     supportedInputMethods Input methods (Chrome OS only).
   * @param {string|undefined} currentInputMethodId ID of the currently used
   *     input method (Chrome OS only).
   * @private
   */
  createModel_: function(
      supportedLanguages, translateTarget, supportedInputMethods,
      currentInputMethodId) {
    // Populate the hash map of supported languages.
    for (var i = 0; i < supportedLanguages.length; i++) {
      var language = supportedLanguages[i];
      language.supportsUI = !!language.supportsUI;
      language.supportsTranslate = !!language.supportsTranslate;
      language.supportsSpellcheck = !!language.supportsSpellcheck;
      this.supportedLanguageMap_.set(language.code, language);
    }

    if (supportedInputMethods) {
      // Populate the hash map of supported input methods.
      for (var j = 0; j < supportedInputMethods.length; j++) {
        var inputMethod = supportedInputMethods[j];
        inputMethod.enabled = !!inputMethod.enabled;
        // Add the input method to the map of IDs.
        this.supportedInputMethodMap_.set(inputMethod.id, inputMethod);
        // Add the input method to the list of input methods for each language
        // it supports.
        for (var k = 0; k < inputMethod.languageCodes.length; k++) {
          var languageCode = inputMethod.languageCodes[k];
          if (!this.supportedLanguageMap_.has(languageCode))
            continue;
          if (!this.languageInputMethods_.has(languageCode))
            this.languageInputMethods_.set(languageCode, [inputMethod]);
          else
            this.languageInputMethods_.get(languageCode).push(inputMethod);
        }
      }
    }

    var prospectiveUILanguage;
    if (cr.isChromeOS || cr.isWindows) {
      prospectiveUILanguage =
          /** @type {string} */ (this.getPref('intl.app_locale').value) ||
          this.originalProspectiveUILanguage_;
    }

    // Create a list of enabled languages from the supported languages.
    var enabledLanguageStates =
        this.getEnabledLanguageStates_(translateTarget, prospectiveUILanguage);
    // Populate the hash set of enabled languages.
    for (var l = 0; l < enabledLanguageStates.length; l++)
      this.enabledLanguageSet_.add(enabledLanguageStates[l].language.code);

    var model = /** @type {!LanguagesModel} */ ({
      supported: supportedLanguages,
      enabled: enabledLanguageStates,
      translateTarget: translateTarget,
    });

    if (cr.isChromeOS || cr.isWindows)
      model.prospectiveUILanguage = prospectiveUILanguage;

    if (cr.isChromeOS) {
      model.inputMethods = /** @type {!InputMethodsModel} */ ({
        supported: supportedInputMethods,
        enabled: this.getEnabledInputMethods_(),
        currentId: currentInputMethodId,
      });
    }

    // Initialize the Polymer languages model.
    this._setLanguages(model);
  },

  /**
   * Returns a list of LanguageStates for each enabled language in the supported
   * languages list.
   * @param {string} translateTarget Language code of the default translate
   *     target language.
   * @param {(string|undefined)} prospectiveUILanguage Prospective UI display
   *     language. Only defined on Windows and Chrome OS.
   * @return {!Array<!LanguageState>}
   * @private
   */
  getEnabledLanguageStates_: function(translateTarget, prospectiveUILanguage) {
    assert(CrSettingsPrefs.isInitialized);

    var pref = this.getPref(preferredLanguagesPrefName);
    var enabledLanguageCodes = pref.value.split(',');
    var spellCheckPref = this.getPref('spellcheck.dictionaries');
    var spellCheckSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */ (spellCheckPref.value));

    var translateBlockedPref = this.getPref('translate_blocked_languages');
    var translateBlockedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */ (translateBlockedPref.value));

    var enabledLanguageStates = [];
    for (var i = 0; i < enabledLanguageCodes.length; i++) {
      var code = enabledLanguageCodes[i];
      var language = this.supportedLanguageMap_.get(code);
      // Skip unsupported languages.
      if (!language)
        continue;
      var languageState = /** @type {LanguageState} */ ({});
      languageState.language = language;
      languageState.spellCheckEnabled = !!spellCheckSet.has(code);
      // Translate is considered disabled if this language maps to any translate
      // language that is blocked.
      var translateCode = this.convertLanguageCodeForTranslate(code);
      languageState.translateEnabled = !!language.supportsTranslate &&
          !translateBlockedSet.has(translateCode) &&
          translateCode != translateTarget &&
          (!prospectiveUILanguage || code != prospectiveUILanguage);
      enabledLanguageStates.push(languageState);
    }
    return enabledLanguageStates;
  },

  /**
   * Returns a list of enabled input methods.
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   * @private
   */
  getEnabledInputMethods_: function() {
    assert(cr.isChromeOS);
    assert(CrSettingsPrefs.isInitialized);

    var enabledInputMethodIds =
        this.getPref('settings.language.preload_engines').value.split(',');
    enabledInputMethodIds = enabledInputMethodIds.concat(
        this.getPref('settings.language.enabled_extension_imes')
            .value.split(','));

    // Return only supported input methods.
    return enabledInputMethodIds
        .map(id => this.supportedInputMethodMap_.get(id))
        .filter(function(inputMethod) {
          return !!inputMethod;
        });
  },

  /** @private */
  updateEnabledInputMethods_: function() {
    assert(cr.isChromeOS);
    var enabledInputMethods = this.getEnabledInputMethods_();
    var enabledInputMethodSet = this.makeSetFromArray_(enabledInputMethods);

    for (var i = 0; i < this.languages.inputMethods.supported.length; i++) {
      this.set(
          'languages.inputMethods.supported.' + i + '.enabled',
          enabledInputMethodSet.has(this.languages.inputMethods.supported[i]));
    }
    this.set('languages.inputMethods.enabled', enabledInputMethods);
  },

  /**
   * Updates the |removable| property of the enabled language states based
   * on what other languages and input methods are enabled.
   * @private
   */
  updateRemovableLanguages_: function() {
    assert(this.languages);
    // TODO(michaelpg): Enabled input methods can affect which languages are
    // removable, so run updateEnabledInputMethods_ first (if it has been
    // scheduled).
    if (cr.isChromeOS)
      this.updateEnabledInputMethods_();

    for (var i = 0; i < this.languages.enabled.length; i++) {
      var languageState = this.languages.enabled[i];
      this.set(
          'languages.enabled.' + i + '.removable',
          this.canDisableLanguage(languageState.language.code));
    }
  },

  /**
   * Creates a Set from the elements of the array.
   * @param {!Array<T>} list
   * @return {!Set<T>}
   * @template T
   * @private
   */
  makeSetFromArray_: function(list) {
    return new Set(list);
  },

  // LanguageHelper implementation.
  // TODO(michaelpg): replace duplicate docs with @override once b/24294625
  // is fixed.

  /** @return {!Promise} */
  whenReady: function() {
    return this.resolver_.promise;
  },

  // <if expr="chromeos or is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   * @param {string} languageCode
   */
  setProspectiveUILanguage: function(languageCode) {
    this.browserProxy_.setProspectiveUILanguage(languageCode);
  },

  /**
   * True if the prospective UI language was changed from its starting value.
   * @return {boolean}
   */
  requiresRestart: function() {
    return this.originalProspectiveUILanguage_ !=
        this.languages.prospectiveUILanguage;
  },
  // </if>

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

    this.languageSettingsPrivate_.enableLanguage(languageCode);
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

    if (cr.isChromeOS) {
      // Remove input methods that don't support any other enabled language.
      var inputMethods = this.languageInputMethods_.get(languageCode) || [];
      for (var i = 0; i < inputMethods.length; i++) {
        var inputMethod = inputMethods[i];
        var supportsOtherEnabledLanguages = inputMethod.languageCodes.some(
            otherLanguageCode => otherLanguageCode != languageCode &&
                this.isLanguageEnabled(otherLanguageCode));
        if (!supportsOtherEnabledLanguages)
          this.removeInputMethod(inputMethod.id);
      }
    }

    // Remove the language from preferred languages.
    this.languageSettingsPrivate_.disableLanguage(languageCode);
  },

  /**
   * @param {string} languageCode Language code for an enabled language.
   * @return {boolean}
   */
  canDisableLanguage: function(languageCode) {
    // Cannot disable the prospective UI language.
    if (languageCode == this.languages.prospectiveUILanguage)
      return false;

    // Cannot disable the only enabled language.
    if (this.languages.enabled.length == 1)
      return false;

    if (!cr.isChromeOS)
      return true;

    // If this is the only enabled language that is supported by all enabled
    // component IMEs, it cannot be disabled because we need those IMEs.
    var otherInputMethodsEnabled =
        this.languages.enabled.some(function(languageState) {
          var otherLanguageCode = languageState.language.code;
          if (otherLanguageCode == languageCode)
            return false;
          var inputMethods = this.languageInputMethods_.get(otherLanguageCode);
          return inputMethods && inputMethods.some(function(inputMethod) {
            return this.isComponentIme(inputMethod) &&
                this.supportedInputMethodMap_.get(inputMethod.id).enabled;
          }, this);
        }, this);
    return otherInputMethodsEnabled;
  },

  /**
   * Moves the language in the list of enabled languages by the given offset.
   * @param {string} languageCode
   * @param {number} offset Negative offset moves the language toward the front
   *     of the list. A Positive one moves the language toward the back.
   */
  moveLanguage: function(languageCode, offset) {
    if (!CrSettingsPrefs.isInitialized)
      return;

    var languageCodes =
        this.getPref(preferredLanguagesPrefName).value.split(',');

    var originalIndex = languageCodes.indexOf(languageCode);
    var newIndex = originalIndex;
    var direction = Math.sign(offset);
    var distance = Math.abs(offset);

    // Step over the distance to find the target index.
    while (distance > 0) {
      newIndex += direction;
      if (newIndex < 0 || newIndex >= languageCodes.length)
        return;

      // Skip over non-enabled languages, since they don't appear in the list
      // (but we don't want to remove them).
      if (this.enabledLanguageSet_.has(languageCodes[newIndex]))
        distance--;
    }

    languageCodes[originalIndex] = languageCodes[newIndex];
    languageCodes[newIndex] = languageCode;
    this.setPrefValue(preferredLanguagesPrefName, languageCodes.join(','));
  },

  /**
   * Moves the language directly to the front of the list of enabled languages.
   * @param {string} languageCode
   */
  moveLanguageToFront: function(languageCode) {
    if (!CrSettingsPrefs.isInitialized)
      return;

    var languageCodes =
        this.getPref(preferredLanguagesPrefName).value.split(',');
    var originalIndex = languageCodes.indexOf(languageCode);
    assert(originalIndex != -1);

    languageCodes.splice(originalIndex, 1);
    languageCodes.unshift(languageCode);

    this.setPrefValue(preferredLanguagesPrefName, languageCodes.join(','));
  },

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   * @param {string} languageCode
   */
  enableTranslateLanguage: function(languageCode) {
    this.languageSettingsPrivate_.setEnableTranslationForLanguage(
        languageCode, true);
  },

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   * @param {string} languageCode
   */
  disableTranslateLanguage: function(languageCode) {
    this.languageSettingsPrivate_.setEnableTranslationForLanguage(
        languageCode, false);
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
   * Given a language code, returns just the base language. E.g., converts
   * 'en-GB' to 'en'.
   * @param {string} languageCode
   * @return {string}
   */
  getLanguageCodeWithoutRegion: function(languageCode) {
    // The Norwegian languages fall under the 'no' macrolanguage.
    if (languageCode == 'nb' || languageCode == 'nn')
      return 'no';

    // Match the characters before the hyphen.
    var result = languageCode.match(/^([^-]+)-?/);
    assert(result.length == 2);
    return result[1];
  },

  /**
   * @param {string} languageCode
   * @return {!chrome.languageSettingsPrivate.Language|undefined}
   */
  getLanguage: function(languageCode) {
    return this.supportedLanguageMap_.get(languageCode);
  },

  // <if expr="chromeos">
  /** @param {string} id */
  addInputMethod: function(id) {
    if (!this.supportedInputMethodMap_.has(id))
      return;
    this.languageSettingsPrivate_.addInputMethod(id);
  },

  /** @param {string} id */
  removeInputMethod: function(id) {
    if (!this.supportedInputMethodMap_.has(id))
      return;
    this.languageSettingsPrivate_.removeInputMethod(id);
  },

  /** @param {string} id */
  setCurrentInputMethod: function(id) {
    this.inputMethodPrivate_.setCurrentInputMethod(id);
  },

  /**
   * @param {string} languageCode
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   */
  getInputMethodsForLanguage: function(languageCode) {
    return this.languageInputMethods_.get(languageCode) || [];
  },

  /**
   * @param {!chrome.languageSettingsPrivate.InputMethod} inputMethod
   * @return {boolean}
   */
  isComponentIme: function(inputMethod) {
    return inputMethod.id.startsWith('_comp_');
  },

  /** @param {string} id Input method ID. */
  openInputMethodOptions: function(id) {
    this.inputMethodPrivate_.openOptionsPage(id);
  },

  /** @param {string} id New current input method ID. */
  onInputMethodChanged_: function(id) {
    this.set('languages.inputMethods.currentId', id);
  },

  /** @param {string} id Added input method ID. */
  onInputMethodAdded_: function(id) {
    this.updateEnabledInputMethods_();
  },

  /** @param {string} id Removed input method ID. */
  onInputMethodRemoved_: function(id) {
    this.updateEnabledInputMethods_();
  },
  // </if>
});
})();
