// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-settings-languages-page' is the settings page
 * for language and input method settings.
 *
 * @group Chrome Settings Elements
 * @element cr-settings-languages-page
 */
Polymer({
  is: 'cr-settings-languages-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    dummyLanguages_: {
      type: Array,
      value: function() {
        return [{code: 'en', displayName: 'English', supportsSpellcheck: true},
                {code: 'es', displayName: 'Spanish', supportsSpellcheck: true},
                {code: 'ru', displayName: 'Russian', supportsSpellcheck: true},
                {code: 'ar', displayName: 'Arabic'}];
      },
    },

    dummyInputMethods_: {
      type: Array,
      value: function() {
        return [{id: 'us', name: 'US Keyboard'},
                {id: 'fr', name: 'French Keyboard'}];
      },
    },

    dummyAppLocale_: {
      type: String,
      value: 'en',
    },

    dummyCurrentInputMethod_: {
      type: String,
      value: 'us',
    },

    dummySpellcheckDictionaries_: {
      type: Array,
      value: function() {
        return ['en', 'es', 'ru'];
      },
    },

    route: String,

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: false,
      readOnly: true
    },

    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'languages',
      readOnly: true
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() {
        return loadTimeData.getString('languagesPageTitle');
      },
    },

    /**
     * Name of the 'iron-icon' to be shown in the settings-page-header.
     */
    icon: {
      type: String,
      value: 'language',
      readOnly: true
    },
  },

  /**
   * @param {!Array<!Language>} languages
   * @return {!Array<!Language>} The languages from |languages| which support
   *     spell check.
   * @private
   */
  getSpellcheckLanguages_: function(languages) {
    return languages.filter(function(language) {
      return language.supportsSpellcheck;
    });
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} dummyAppLocale A fake app locale.
   * @return {boolean} True if the given language matches the app locale pref
   *     (which may be different from the actual app locale if it was changed).
   * @private
   */
  isUILanguage_: function(languageCode, dummyAppLocale) {
    return languageCode == dummyAppLocale;
  },

  /**
   * @param {string} id The input method ID.
   * @param {string} currentId The ID of the currently enabled input method.
   * @return {boolean} True if the IDs match.
   * @private
   */
  isCurrentInputMethod_: function(id, currentId) {
    assert(cr.isChromeOS);
    return id == currentId;
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {!Array<string>} spellcheckDictionaries The list of languages
   *     for which spell check is enabled.
   * @return {boolean} True if spell check is enabled for the language.
   * @private
   */
  isSpellcheckEnabled_: function(languageCode, spellcheckDictionaries) {
    return spellcheckDictionaries.indexOf(languageCode) != -1;
  },
});
