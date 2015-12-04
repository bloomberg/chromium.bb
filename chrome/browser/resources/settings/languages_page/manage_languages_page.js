// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-manage-languages-page' is a sub-page for enabling
 * and disabling languages.
 *
 * @group Chrome Settings Elements
 * @element settings-manage-languages-page
 */
Polymer({
  is: 'settings-manage-languages-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * @type {!LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
    },

    /**
     * @private {!Array<!{code: string, displayName: string,
     *                    nativeDisplayName: string, enabled: boolean}>|
     *           undefined}
     */
    availableLanguages_: Array,
  },

  /** @private {!LanguageHelper} */
  languageHelper_: LanguageHelperImpl.getInstance(),

  observers: [
    'enabledLanguagesChanged_(languages.enabledLanguages.*)',
  ],

  /**
   * Handler for removing a language.
   * @param {!{model: !{item: !LanguageInfo}}} e
   * @private
   */
  onRemoveLanguageTap_: function(e) {
    this.languageHelper_.disableLanguage(e.model.item.language.code);
  },

  /**
   * Handler for adding a language.
   * @param {!{model: {item: !chrome.languageSettingsPrivate.Language}}} e
   * @private
   */
  onAddLanguageTap_: function(e) {
    this.languageHelper_.enableLanguage(e.model.item.code);
  },

  /**
   * True if a language is not the current or prospective UI language.
   * @param {string} languageCode
   * @param {string} prospectiveUILanguageCode
   * @return {boolean}
   * @private
   */
  canRemoveLanguage_: function(languageCode, prospectiveUILanguageCode) {
    if (languageCode == navigator.language ||
        languageCode == prospectiveUILanguageCode) {
      return false;
    }
    assert(this.languages.enabledLanguages.length > 1);
    return true;
  },

  /**
   * Updates the available languages that are bound to the iron-list.
   * @private
   */
  enabledLanguagesChanged_: function() {
    if (!this.availableLanguages_) {
      var availableLanguages = [];
      for (var i = 0; i < this.languages.supportedLanguages.length; i++) {
        var language = this.languages.supportedLanguages[i];
        availableLanguages.push({
          code: language.code,
          displayName: language.displayName,
          nativeDisplayName: language.nativeDisplayName,
          enabled: this.languageHelper_.isLanguageEnabled(language.code),
        });
      }
      // Set the Polymer property after building the full array.
      this.availableLanguages_ = availableLanguages;
    } else {
      // Update the available languages in place.
      for (var i = 0; i < this.availableLanguages_.length; i++) {
        this.set('availableLanguages_.' + i + '.enabled',
                 this.languageHelper_.isLanguageEnabled(
                      this.availableLanguages_[i].code));
      }
    }
  },
});
