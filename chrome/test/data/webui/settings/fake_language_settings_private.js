// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.languageSettingsPrivate
 * for testing.
 */
cr.define('settings', function() {
  /**
   * Returns a function that calls assertNotReached with the given name.
   * Necessary to include the callee in the stack trace.
   * @param {string} name
   * @return {function()}
   */
  function wrapAssertNotReached(name) {
    return function() {
      assertNotReached('Not implemented in fake: ' + name);
    };
  }

  /**
   * Fake of the chrome.languageSettingsPrivate API.
   * @constructor
   * @implements {LanguageSettingsPrivate}
   */
  function FakeLanguageSettingsPrivate() {
    /** @type {!Array<!chrome.languageSettingsPrivate.Language>} */
    this.languages = [{
      // English and some variants.
      code: 'en',
      displayName: 'English',
      nativeDisplayName: 'English',
      supportsTranslate: true,
    }, {
      code: 'en-CA',
      displayName: 'English (Canada)',
      nativeDisplayName: 'English (Canada)',
      supportsSpellcheck: true,
      supportsUI: true,
    }, {
      code: 'en-US',
      displayName: 'English (United States)',
      nativeDisplayName: 'English (United States)',
      supportsSpellcheck: true,
      supportsUI: true,
    }, {
      // A standalone language.
      code: "sw",
      displayName: "Swahili",
      nativeDisplayName: "Kiswahili",
      supportsTranslate: true,
      supportsUI: true,
    }, {
      // A standalone language that doesn't support anything.
      code: "tk",
      displayName: "Turkmen",
      nativeDisplayName: "Turkmen"
    }, {
      // Edge cases:
      // Norwegian is the macrolanguage for "nb" (see below).
      code: "no",
      displayName: "Norwegian",
      nativeDisplayName: "norsk",
      supportsTranslate: true,
    }, {
      // Norwegian language codes don't start with "no-" but should still
      // fall under the Norwegian macrolanguage.
      // TODO(michaelpg): Test this is ordered correctly.
      code: "nb",
      displayName: "Norwegian Bokmål",
      nativeDisplayName: "norsk bokmål",
      supportsSpellcheck: true,
      supportsUI: true,
    }];

    /** @type {!Array<!chrome.languageSettingsPrivate.InputMethod>} */
    this.componentExtensionImes = [{
      id: '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us::eng',
      displayName: 'US keyboard',
      languageCodes: ['en', 'en-US'],
      enabled: true,
    }, {
      id: '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us:dvorak:eng',
      displayName: 'US Dvorak keyboard',
      languageCodes: ['en', 'en-US'],
      enabled: true,
    }];
  }

  FakeLanguageSettingsPrivate.prototype = {
    // Methods for use in testing.

    /** @param {SettingsPrefsElement} */
    setSettingsPrefs: function(settingsPrefs) {
      this.settingsPrefs_ = settingsPrefs;
    },

    // LanguageSettingsPrivate fake.

    /**
     * Gets languages available for translate, spell checking, input and locale.
     * @param {function(!Array<!chrome.languageSettingsPrivate.Language>)}
     *     callback
     */
    getLanguageList: function(callback) {
      setTimeout(function() {
        callback(JSON.parse(JSON.stringify(this.languages)));
      }.bind(this));
    },

    /**
     * Sets the accepted languages, used to decide which languages to translate,
     * generate the Accept-Language header, etc.
     * @param {!Array<string>} languageCodes
     */
    setLanguageList: function(languageCodes) {
      var languages = languageCodes.join(',');
      this.settingsPrefs_.set('prefs.intl.accept_languages.value', languages);
      if (cr.isChromeOS) {
        this.settingsPrefs_.set(
            'prefs.settings.language.preferred_languages.value', languages);
      }
    },

    /**
     * Gets the current status of the chosen spell check dictionaries.
     * @param {function(!Array<
     *     !chrome.languageSettingsPrivate.SpellcheckDictionaryStatus>):void}
     *     callback
     */
    getSpellcheckDictionaryStatuses:
        wrapAssertNotReached('getSpellcheckDictionaryStatuses'),

    /**
     * Gets the custom spell check words, in sorted order.
     * @param {function(!Array<string>):void} callback
     */
    getSpellcheckWords: wrapAssertNotReached('getSpellcheckWords'),

    /**
     * Adds a word to the custom dictionary.
     * @param {string} word
     */
    addSpellcheckWord: wrapAssertNotReached('addSpellcheckWord'),

    /**
     * Removes a word from the custom dictionary.
     * @param {string} word
     */
    removeSpellcheckWord: wrapAssertNotReached('removeSpellcheckWord'),

    /**
     * Gets the translate target language (in most cases, the display locale).
     * @param {function(string):void} callback
     */
    getTranslateTargetLanguage: function(callback) {
      setTimeout(callback.bind(null, 'en'));
    },

    /**
     * Gets all supported input methods, including third-party IMEs. Chrome OS
     * only.
     * @param {function(!chrome.languageSettingsPrivate.InputMethodLists):void}
     *     callback
     */
    getInputMethodLists: function(callback) {
      if (!cr.isChromeOS)
        assertNotReached();
      callback({
        componentExtensionImes:
            JSON.parse(JSON.stringify(this.componentExtensionImes)),
        thirdPartyExtensionImes: [],
      });
    },

    /**
     * Adds the input method to the current user's list of enabled input
     * methods, enabling the input method for the current user. Chrome OS only.
     * @param {string} inputMethodId
     */
    addInputMethod: wrapAssertNotReached('addInputMethod'),

    /**
     * Removes the input method from the current user's list of enabled input
     * methods, disabling the input method for the current user. Chrome OS only.
     * @param {string} inputMethodId
     */
    removeInputMethod: function(inputMethodId) {
      assert(cr.isChromeOS);
      var inputMethod = this.componentExtensionImes.find(function(ime) {
        return ime.id == inputMethodId;
      });
      assertTrue(!!inputMethod);
      inputMethod.enabled = false;
      this.settingsPrefs_.set(
          'prefs.settings.language.preload_engines.value',
          this.settingsPrefs_.prefs.settings.language.preload_engines.value
              .replace(inputMethodId, ''));
    },

    /**
     * Called when the pref for the dictionaries used for spell checking changes
     * or the status of one of the spell check dictionaries changes.
     * @type {!ChromeEvent}
     */
    onSpellcheckDictionariesChanged: new FakeChromeEvent(),

    /**
     * Called when words are added to and/or removed from the custom spell check
     * dictionary.
     * @type {!ChromeEvent}
     */
    onCustomDictionaryChanged: new FakeChromeEvent(),

    /**
     * Called when an input method is added.
     * @type {!ChromeEvent}
     */
    onInputMethodAdded: new FakeChromeEvent(),

    /**
     * Called when an input method is removed.
     * @type {!ChromeEvent}
     */
    onInputMethodRemoved: new FakeChromeEvent(),
  };

  return {FakeLanguageSettingsPrivate: FakeLanguageSettingsPrivate};
});
