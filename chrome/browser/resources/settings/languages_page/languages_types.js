// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for dictionaries and interfaces used by
 * language settings.
 */

/**
 * Current properties of a language.
 * @typedef {{spellCheckEnabled: boolean, translateEnabled: boolean,
 *            removable: boolean}} */
var LanguageState;

/**
 * Information about a language including intrinsic information (|language|)
 * and the |state| of the language.
 * @typedef {{language: !chrome.languageSettingsPrivate.Language,
 *            state: !LanguageState}}
 */
var LanguageInfo;

/**
 * Languages data to expose to consumers.
 * supportedLanguages: an array of languages, ordered alphabetically.
 * enabledLanguages: an array of enabled language info, ordered by preference.
 * translateTarget: the default language to translate into.
 * @typedef {{
 *   supportedLanguages: !Array<!chrome.languageSettingsPrivate.Language>,
 *   enabledLanguages: !Array<!LanguageInfo>,
 *   translateTarget: string
 * }}
 */
var LanguagesModel;

/**
 * Helper methods implemented by settings-languages-singleton. The nature of
 * the interaction between the singleton Polymer element and the |languages|
 * properties kept in sync is hidden from the consumer, which can just treat
 * these methods as a handy interface.
 * @interface
 */
var LanguageHelper = function() {};

LanguageHelper.prototype = {

<if expr="chromeos or is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   * @param {string} languageCode
   */
  setUILanguage: assertNotReached,

  /** Resets the prospective UI language back to the actual UI language. */
  resetUILanguage: assertNotReached,

  /**
   * Returns the "prospective" UI language, i.e. the one to be used on next
   * restart. If the pref is not set, the current UI language is also the
   * "prospective" language.
   * @return {string} Language code of the prospective UI language.
   */
  getProspectiveUILanguage: assertNotReached,
</if>

  /**
   * @param {string} languageCode
   * @return {boolean}
   */
  isLanguageEnabled: assertNotReached,

  /**
   * Enables the language, making it available for spell check and input.
   * @param {string} languageCode
   */
  enableLanguage: assertNotReached,

  /**
   * Disables the language.
   * @param {string} languageCode
   */
  disableLanguage: assertNotReached,

  /**
   * @param {string} languageCode Language code for an enabled language.
   * @return {boolean}
   */
  canDisableLanguage: assertNotReached,

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   * @param {string} languageCode
   */
  enableTranslateLanguage: assertNotReached,

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   * @param {string} languageCode
   */
  disableTranslateLanguage: assertNotReached,

  /**
   * Enables or disables spell check for the given language.
   * @param {string} languageCode
   * @param {boolean} enable
   */
  toggleSpellCheck: assertNotReached,

  /**
   * Converts the language code for translate. There are some differences
   * between the language set the Translate server uses and that for
   * Accept-Language.
   * @param {string} languageCode
   * @return {string} The converted language code.
   */
  convertLanguageCodeForTranslate: assertNotReached,

  /**
   * @param {string} languageCode
   * @return {!chrome.languageSettingsPrivate.Language|undefined}
   */
  getLanguage: assertNotReached,
};
