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
(function() {
'use strict';

Polymer({
  is: 'cr-settings-languages-page',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Read-only reference to the languages model provided by the
     * 'cr-settings-languages' instance.
     * @type {LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
    },
  },

  /**
   * Handler for clicking a language on the main page, which selects the
   * language as the prospective UI language on Chrome OS and Windows.
   * @param {!{model: !{item: !LanguageInfo}}} e
   */
  onLanguageTap_: function(e) {
    // Set the prospective UI language. This won't take effect until a restart.
    if (e.model.item.language.supportsUI)
      this.$.languages.setUILanguage(e.model.item.language.code);
  },

  /**
   * Handler for enabling or disabling spell check.
   * @param {!{target: Element, model: !{item: !LanguageInfo}}} e
   */
  onSpellCheckChange_: function(e) {
    this.$.languages.toggleSpellCheck(e.model.item.language.code,
                                      e.target.checked);
  },

  /** @private */
  onBackTap_: function() {
    this.$.pages.back();
  },

  /**
   * Opens the Manage Languages page.
   * @private
   */
  onManageLanguagesTap_: function() {
    this.$.pages.setSubpageChain(['manage-languages']);
    // HACK(michaelpg): This is necessary to show the list when navigating to
    // the sub-page. Remove when PolymerElements/neon-animation#60 is fixed.
    /** @type {{_render: function()}} */(this.$.manageLanguagesPage.$.list)
        ._render();
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} appLocale The prospective UI language.
   * @return {boolean} True if the given language matches the app locale pref
   *     (which may be different from the actual app locale).
   * @private
   */
  isUILanguage_: function(languageCode, appLocale) {
    // Check the current language if the locale pref hasn't been set.
    if (!appLocale)
      appLocale = navigator.language;
    return languageCode == appLocale;
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
});
})();
