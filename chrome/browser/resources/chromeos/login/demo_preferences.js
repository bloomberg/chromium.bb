// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'demo-preferences-md',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  properties: {
    /** Currently selected system language (display name). */
    currentLanguage: {
      type: String,
      value: '',
    },

    /** Currently selected input method (display name). */
    currentKeyboard: {
      type: String,
      value: '',
    },

    /**
     * List of languages for language selector dropdown.
     * @type {!Array<!OobeTypes.LanguageDsc>}
     */
    languages: {
      type: Array,
      observer: 'onLanguagesChanged_',
    },

    /**
     * List of keyboards for keyboard selector dropdown.
     * @type {!Array<!OobeTypes.IMEDsc>}
     */
    keyboards: {
      type: Array,
      observer: 'onKeyboardsChanged_',
    },
  },

  /** @override */
  ready: function() {
    this.i18nUpdateLocale();

    assert(loadTimeData);
    var languageList = loadTimeData.getValue('languageList');
    this.setLanguageList_(languageList);

    var inputMethodsList = loadTimeData.getValue('inputMethodsList');
    this.setInputMethods_(inputMethodsList);
  },

  /** Called after resources are updated. */
  updateLocalizedContent: function() {
    this.i18nUpdateLocale();
  },

  /**
   * Sets language list.
   * @param {!Array<!OobeTypes.LanguageDsc>} languages
   * @private
   */
  setLanguageList_: function(languages) {
    this.languages = languages;
  },

  /**
   * Sets input methods.
   * @param {!Array<!OobeTypes.IMEDsc>} inputMethods
   * @private
   */
  setInputMethods_: function(inputMethods) {
    this.keyboards = inputMethods;
  },

  /**
   * Sets selected keyboard.
   * @param {string} keyboardId
   * @private
   */
  setSelectedKeyboard_: function(keyboardId) {
    var found = false;
    for (var keyboard of this.keyboards) {
      if (keyboard.value != keyboardId) {
        keyboard.selected = false;
        continue;
      }
      keyboard.selected = true;
      found = true;
    }
    if (!found)
      return;

    // Force i18n-dropdown to refresh.
    this.keyboards = this.keyboards.slice();
    this.onKeyboardsChanged_();
  },

  /**
   * Handle language selection.
   * @param {!{detail: {!OobeTypes.LanguageDsc}}} event
   * @private
   */
  onLanguageSelected_: function(event) {
    var item = event.detail;
    var languageId = item.value;
    this.currentLanguage = item.title;
    this.screen.onLanguageSelected_(languageId);
  },

  /**
   * Handle keyboard layout selection.
   * @param {!{detail: {!OobeTypes.IMEDsc}}} event
   * @private
   */
  onKeyboardSelected_: function(event) {
    var item = event.detail;
    var inputMethodId = item.value;
    this.currentKeyboard = item.title;
    this.screen.onKeyboardSelected_(inputMethodId);
  },

  /**
   * Language changes handler.
   * @private
   */
  onLanguagesChanged_: function() {
    this.currentLanguage = getSelectedTitle(this.languages);
  },

  /**
   * Keyboard changes handler.
   * @private
   */
  onKeyboardsChanged_: function() {
    this.currentKeyboard = getSelectedTitle(this.keyboards);
  },

  /**
   * Back button click handler.
   * @private
   */
  onBackClicked_: function() {
    chrome.send('login.DemoPreferencesScreen.userActed', ['close-setup']);
  },

  /**
   * Next button click handler.
   * @private
   */
  onNextClicked_: function() {
    chrome.send('login.DemoPreferencesScreen.userActed', ['continue-setup']);
  },

});
