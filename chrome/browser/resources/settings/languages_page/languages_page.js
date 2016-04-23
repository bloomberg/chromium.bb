// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages-page' is the settings page
 * for language and input method settings.
 */
(function() {
'use strict';

Polymer({
  is: 'settings-languages-page',

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
     * 'settings-languages' instance.
     * @type {!LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
    },

    /** @private */
    inputMethodsSecondary_: {
      type: String,
      value: 'Placeholder, e.g. US keyboard',
    },

    /** @private */
    spellCheckSecondary_: {
      type: String,
      value: 'Placeholder, e.g. English (United States)',
    },

    /**
     * The language to display the details for.
     * @type {!LanguageState|undefined}
     * @private
     */
    detailLanguage_: Object,

    /** @private {!LanguageHelper} */
    languageHelper_: Object,
  },

  /** @override */
  created: function() {
    this.languageHelper_ = LanguageHelperImpl.getInstance();
  },

  /**
   * Handler for clicking a language on the main page, which selects the
   * language as the prospective UI language on Chrome OS and Windows.
   * @param {!{model: !{item: !LanguageState}}} e
   */
  onLanguageTap_: function(e) {
    // Only change the UI language on platforms that allow it.
    if (!cr.isChromeOS && !cr.isWindows)
      return;

    // Taps on the paper-icon-button are handled in onShowLanguageDetailTap_.
    if (e.target.tagName == 'PAPER-ICON-BUTTON')
      return;

    // Set the prospective UI language. This won't take effect until a restart.
    if (e.model.item.language.supportsUI)
      this.languageHelper_.setUILanguage(e.model.item.language.code);
  },

  /**
   * Handler for enabling or disabling spell check.
   * @param {!{target: Element, model: !{item: !LanguageState}}} e
   */
  onSpellCheckChange_: function(e) {
    this.languageHelper_.toggleSpellCheck(e.model.item.language.code,
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
    this.forceRenderList_('settings-manage-languages-page');
  },

  /**
   * Opens the Language Detail page for the language.
   * @param {!{model: !{item: !LanguageState}}} e
   * @private
   */
  onShowLanguageDetailTap_: function(e) {
    this.detailLanguage_ = e.model.item;
    this.$.pages.setSubpageChain(['language-detail']);
  },

  /**
   * Returns the enabled languages which support spell check.
   * @return {!Array<!LanguageState>}
   * @private
   */
  spellCheckLanguages_: function() {
    assert(!cr.isMac);
    return this.languages.enabled.filter(function(languageState) {
      return languageState.language.supportsSpellcheck;
    });
  },

  /**
   * Opens the Custom Dictionary page.
   * @private
   */
  onEditDictionaryTap_: function() {
    assert(!cr.isMac);
    this.$.pages.setSubpageChain(['edit-dictionary']);
    this.forceRenderList_('settings-edit-dictionary-page');
  },

  /**
   * Checks whether the prospective UI language (the pref that indicates what
   * language to use in Chrome) matches the current language. This pref is only
   * on Chrome OS and Windows; we don't control the UI language elsewhere.
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {boolean} True if the given language matches the prospective UI
   *     pref (which may be different from the actual UI language).
   * @private
   */
  isProspectiveUILanguage_: function(languageCode, prospectiveUILanguage) {
    assert(cr.isChromeOS || cr.isWindows);
    return languageCode == this.languageHelper_.getProspectiveUILanguage();
  },

   /**
    * @return {string}
    * @private
    */
  getProspectiveUILanguageName_: function() {
    return this.languageHelper_.getLanguage(
        this.languageHelper_.getProspectiveUILanguage()).displayName;
  },

  /**
   * Returns either the "selected" class, if the language matches the
   * prospective UI language, or an empty string. Languages can only be
   * selected on Chrome OS and Windows.
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {string} The class name for the language item.
   * @private
   */
  getLanguageItemClass_: function(languageCode, prospectiveUILanguage) {
    if ((cr.isChromeOS || cr.isWindows) &&
        this.isProspectiveUILanguage_(languageCode, prospectiveUILanguage)) {
      return 'selected';
    }
    return '';
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
   * @param {string} id The input method ID.
   * @param {string} currentId The ID of the currently enabled input method.
   * @return {string} The class for the input method item.
   * @private
   */
  getInputMethodItemClass_: function(id, currentId) {
    assert(cr.isChromeOS);
    return this.isCurrentInputMethod_(id, currentId) ? 'selected' : '';
  },

  /**
   * HACK(michaelpg): This is necessary to show the list when navigating to
   * the sub-page. Remove this function when PolymerElements/neon-animation#60
   * is fixed.
   * @param {string} tagName Name of the element containing the <iron-list>.
   */
  forceRenderList_: function(tagName) {
    this.$$(tagName).$$('iron-list').fire('iron-resize');
  },
});
})();
