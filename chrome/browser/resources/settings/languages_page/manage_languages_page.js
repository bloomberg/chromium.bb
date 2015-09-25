// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-settings-manage-languages-page' is a sub-page for enabling
 * and disabling languages.
 *
 * @group Chrome Settings Elements
 * @element cr-settings-manage-languages-page
 */
Polymer({
  is: 'cr-settings-manage-languages-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * @type {LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
    },

    /**
     * @private {Array<{code: string, displayName: string, enabled: boolean}>|
     *           undefined}
     */
    availableLanguages_: Array,
  },

  observers: [
    'enabledLanguagesChanged_(languages.enabledLanguages.*)',
  ],

  /**
   * Handler for removing a language.
   * @param {!{model: !{item: !Language}}} e
   * @private
   */
  onRemoveLanguageTap_: function(e) {
    this.$.languages.disableLanguage(e.model.item.language.code);
  },

  /**
   * Handler for adding a language.
   * @param {!{model: {item: !chrome.languageSettingsPrivate.Language}}} e
   * @private
   */
  onAddLanguageTap_: function(e) {
    this.$.languages.enableLanguage(e.model.item.code);
  },

  /**
   * True if a language is not the prospective UI language or the last remaining
   * language.
   * @param {string} languageCode
   * @param {!Array<!LanguageInfo>} enableLanguage
   * @private
   * @return {boolean}
   */
  canRemoveLanguage_: function(languageCode, enabledLanguages) {
    var appLocale = this.prefs.intl.app_locale.value || navigator.language;
    if (languageCode == appLocale)
      return false;
    if (enabledLanguages.length == 1)
      return false;
    return true;
  },

  /**
   * Updates the available languages to be bound to the iron-list.
   * TODO(michaelpg): Update properties of individual items instead of
   *     rebuilding entire list.
   * @private
   */
  enabledLanguagesChanged_: function() {
    var availableLanguages = [];
    for (var i = 0; i < this.languages.supportedLanguages.length; i++) {
      var language = this.languages.supportedLanguages[i];
      availableLanguages.push({
        code: language.code,
        displayName: language.displayName,
        enabled: this.$.languages.isEnabled(language.code)
      });
    }
    this.availableLanguages_ = availableLanguages;
  },
});
