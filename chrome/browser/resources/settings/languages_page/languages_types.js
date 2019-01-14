// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for dictionaries and interfaces used by
 * language settings.
 */

/**
 * Settings and state for a particular enabled language.
 * @typedef {{
 *   language: !chrome.languageSettingsPrivate.Language,
 *   removable: boolean,
 *   spellCheckEnabled: boolean,
 *   translateEnabled: boolean,
 *   isManaged: boolean,
 *   downloadDictionaryFailureCount: number,
 *   downloadDictionaryStatus:
 *       ?chrome.languageSettingsPrivate.SpellcheckDictionaryStatus,
 * }}
 */
let LanguageState;

/**
 * Settings and state for a policy-enforced spellcheck language.
 * @typedef {{
 *   language: !chrome.languageSettingsPrivate.Language,
 *   isManaged: boolean,
 *   downloadDictionaryFailureCount: number,
 *   downloadDictionaryStatus:
 *       ?chrome.languageSettingsPrivate.SpellcheckDictionaryStatus,
 * }}
 */
let ForcedLanguageState;

/**
 * Input method data to expose to consumers (Chrome OS only).
 * supported: an array of supported input methods set once at initialization.
 * enabled: an array of the currently enabled input methods.
 * currentId: ID of the currently active input method.
 * @typedef {{
 *   supported: !Array<!chrome.languageSettingsPrivate.InputMethod>,
 *   enabled: !Array<!chrome.languageSettingsPrivate.InputMethod>,
 *   currentId: string,
 * }}
 */
let InputMethodsModel;

/**
 * Languages data to expose to consumers.
 * supported: an array of languages, ordered alphabetically, set once
 *     at initialization.
 * enabled: an array of enabled language states, ordered by preference.
 * translateTarget: the default language to translate into.
 * prospectiveUILanguage: the "prospective" UI language, i.e., the one to be
 *     used on next restart. Matches the current UI language preference unless
 *     the user has chosen a different language without restarting. May differ
 *     from the actually used language (navigator.language). Chrome OS and
 *     Windows only.
 * inputMethods: the InputMethodsModel (Chrome OS only).
 * forcedSpellCheckLanguages: an array of spellcheck languages that are not in
 *     |enabled|.
 * @typedef {{
 *   supported: !Array<!chrome.languageSettingsPrivate.Language>,
 *   enabled: !Array<!LanguageState>,
 *   translateTarget: string,
 *   prospectiveUILanguage: (string|undefined),
 *   inputMethods: (!InputMethodsModel|undefined),
 *   forcedSpellCheckLanguages: !Array<!ForcedLanguageState>,
 * }}
 */
let LanguagesModel;

/**
 * Helper methods for reading and writing language settings.
 * @interface
 */
class LanguageHelper {
  /** @return {!Promise} */
  whenReady() {}

  // <if expr="chromeos or is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   * @param {string} languageCode
   */
  setProspectiveUILanguage(languageCode) {}

  /**
   * True if the prospective UI language has been changed.
   * @return {boolean}
   */
  requiresRestart() {}

  // </if>

  /**
   * @return {string} The language code for ARC IMEs.
   */
  getArcImeLanguageCode() {}

  /**
   * @param {string} languageCode
   * @return {boolean}
   */
  isLanguageCodeForArcIme(languageCode) {}

  /**
   * @param {string} languageCode
   * @return {boolean}
   */
  isLanguageEnabled(languageCode) {}

  /**
   * Enables the language, making it available for spell check and input.
   * @param {string} languageCode
   */
  enableLanguage(languageCode) {}

  /**
   * Disables the language.
   * @param {string} languageCode
   */
  disableLanguage(languageCode) {}

  /**
   * Returns true iff provided languageState is the only blocked language.
   * @param {!LanguageState} languageState
   * @return {boolean}
   */
  isOnlyTranslateBlockedLanguage(languageState) {}

  /**
   * Returns true iff provided languageState can be disabled.
   * @param {!LanguageState} languageState
   * @return {boolean}
   */
  canDisableLanguage(languageState) {}

  /**
   * @param {!chrome.languageSettingsPrivate.Language} language
   * @return {boolean} true if the given language can be enabled
   */
  canEnableLanguage(language) {}

  /**
   * Moves the language in the list of enabled languages by the given offset.
   * @param {string} languageCode
   * @param {boolean} upDirection True if we need to move toward the front,
   *     false if we need to move toward the back.
   */
  moveLanguage(languageCode, upDirection) {}

  /**
   * Moves the language directly to the front of the list of enabled languages.
   * @param {string} languageCode
   */
  moveLanguageToFront(languageCode) {}

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   * @param {string} languageCode
   */
  enableTranslateLanguage(languageCode) {}

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   * @param {string} languageCode
   */
  disableTranslateLanguage(languageCode) {}

  /**
   * Enables or disables spell check for the given language.
   * @param {string} languageCode
   * @param {boolean} enable
   */
  toggleSpellCheck(languageCode, enable) {}

  /**
   * Converts the language code for translate. There are some differences
   * between the language set the Translate server uses and that for
   * Accept-Language.
   * @param {string} languageCode
   * @return {string} The converted language code.
   */
  convertLanguageCodeForTranslate(languageCode) {}

  /**
   * Given a language code, returns just the base language. E.g., converts
   * 'en-GB' to 'en'.
   * @param {string} languageCode
   * @return {string}
   */
  getLanguageCodeWithoutRegion(languageCode) {}

  /**
   * @param {string} languageCode
   * @return {!chrome.languageSettingsPrivate.Language|undefined}
   */
  getLanguage(languageCode) {}

  /** @param {string} languageCode */
  retryDownloadDictionary(languageCode) {}

  // <if expr="chromeos">
  /** @param {string} id */
  addInputMethod(id) {}

  /** @param {string} id */
  removeInputMethod(id) {}

  /** @param {string} id */
  setCurrentInputMethod(id) {}

  /**
   * @param {string} languageCode
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   */
  getInputMethodsForLanguage(languageCode) {}

  /**
   * @param {!chrome.languageSettingsPrivate.InputMethod} inputMethod
   * @return {boolean}
   */
  isComponentIme(inputMethod) {}

  /** @param {string} id Input method ID. */
  openInputMethodOptions(id) {}
  // </if>
}
