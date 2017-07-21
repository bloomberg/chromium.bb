// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-add-languages-dialog' is a dialog for enabling
 * languages.
 */
Polymer({
  is: 'settings-add-languages-dialog',

  behaviors: [
    CrScrollableBehavior,
  ],

  properties: {
    /** @type {!LanguagesModel|undefined} */
    languages: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /** @private {!Set<string>} */
    languagesToAdd_: {
      type: Object,
      value: function() {
        return new Set();
      },
    },

    /** @private */
    disableActionButton_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    filterValue_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
  },

  /**
   * @param {!CustomEvent} e
   * @private
   */
  onSearchChanged_: function(e) {
    this.filterValue_ = /** @type {string} */ (e.detail);
  },

  /**
   * @return {!Array<!chrome.languageSettingsPrivate.Language>} A list of
   *     languages to be displayed.
   * @private
   */
  getLanguages_: function() {
    var filterValue =
        this.filterValue_ ? this.filterValue_.toLowerCase() : null;
    return this.languages.supported.filter(function(language) {
      var isAvailableLanguage =
          !this.languageHelper.isLanguageEnabled(language.code);

      if (!isAvailableLanguage)
        return false;

      if (filterValue === null)
        return true;

      return language.displayName.toLowerCase().includes(filterValue) ||
          language.nativeDisplayName.toLowerCase().includes(filterValue);
    }.bind(this));
  },

  /**
   * @param {!chrome.languageSettingsPrivate.Language} language
   * @return {string} The text to be displayed.
   * @private
   */
  getDisplayText_: function(language) {
    var displayText = language.displayName;
    // If the native name is different, add it.
    if (language.displayName != language.nativeDisplayName)
      displayText += ' - ' + language.nativeDisplayName;
    return displayText;
  },

  /**
   * True if the user has chosen to add this language (checked its checkbox).
   * @param {string} languageCode
   * @return {boolean}
   * @private
   */
  willAdd_: function(languageCode) {
    return this.languagesToAdd_.has(languageCode);
  },

  /**
   * Handler for checking or unchecking a language item.
   * @param {!{model: !{item: !chrome.languageSettingsPrivate.Language},
   *           target: !PaperCheckboxElement}} e
   * @private
   */
  onLanguageCheckboxChange_: function(e) {
    // Add or remove the item to the Set. No need to worry about data binding:
    // willAdd_ is called to initialize the checkbox state (in case the
    // iron-list re-uses a previous checkbox), and the checkbox can only be
    // changed after that by user action.
    var language = e.model.item;
    if (e.target.checked)
      this.languagesToAdd_.add(language.code);
    else
      this.languagesToAdd_.delete(language.code);

    this.disableActionButton_ = !this.languagesToAdd_.size;
  },

  /** @private */
  onCancelButtonTap_: function() {
    this.$.dialog.close();
  },

  /**
   * Enables the checked languages.
   * @private
   */
  onActionButtonTap_: function() {
    this.$.dialog.close();
    this.languagesToAdd_.forEach(function(languageCode) {
      this.languageHelper.enableLanguage(languageCode);
    }.bind(this));
  },
});
