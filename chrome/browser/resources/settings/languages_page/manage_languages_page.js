// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-manage-languages-page' is a sub-page for enabling
 * and disabling languages.
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

    /** @private {!LanguageHelper} */
    languageHelper_: Object,
  },

  observers: [
    'enabledLanguagesChanged_(languages.enabledLanguages.*)',
  ],

  /** @override */
  created: function() {
     this.languageHelper_ = LanguageHelperImpl.getInstance();
  },

  /**
   * Handler for removing a language.
   * @param {!{model: !{item: !LanguageInfo}}} e
   * @private
   */
  onRemoveLanguageTap_: function(e) {
    this.languageHelper_.disableLanguage(e.model.item.language.code);
  },

  /**
   * Handler for checking or unchecking a language item.
   * @param {!{model: !{item: !chrome.languageSettingsPrivate.Language},
   *           target: !PaperCheckboxElement}} e
   * @private
   */
  onLanguageCheckboxChange_: function(e) {
    var code = e.model.item.code;
    if (e.target.checked)
      this.languageHelper_.enableLanguage(code);
    else
      this.languageHelper_.disableLanguage(code);
  },

  /**
   * True if a language is not the current or prospective UI language, ie,
   * it could be disabled.
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
