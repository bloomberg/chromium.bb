// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'tts-subpage' is the collapsible section containing
 * text-to-speech settings.
 */

Polymer({
  is: 'settings-tts-subpage',

  behaviors: [WebUIListenerBehavior, I18nBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Available languages.
     */
    languagesToVoices: {
      type: Array,
      value: [],
      notify: true,
    },
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'all-voice-data-updated', this.populateVoiceList_.bind(this));
    chrome.send('getAllTtsVoiceData');
    this.addWebUIListener(
        'tts-extensions-updated', this.populateExtensionList_.bind(this));
    chrome.send('getTtsExtensions');
  },

  /**
   * Ticks for the Speech Rate slider. Non-linear as we expect people
   * to want more control near 1.0.
   * @return Array<{value: number, label: string}>
   * @private
   */
  speechRateTicks_: function() {
    return Array.from(Array(16).keys()).map(x => {
      return x <= 4 ?
          // Linear from rates 0.6 to 1.0
          this.initTick_(x / 10 + .6) :
          // Power function above 1.0 gives more control at lower values.
          this.initTick_(Math.pow(x - 3, 2) / 20 + 1);
    });
  },

  /**
   * Ticks for the Speech Pitch slider. Valid pitches are between 0 and 2,
   * exclusive of 0.
   * @return Array<{value: number, label: string}>
   * @private
   */
  speechPitchTicks_: function() {
    return Array.from(Array(10).keys()).map(x => {
      return this.initTick_(x * .2 + .2);
    });
  },

  /**
   * Ticks for the Speech Volume slider. Valid volumes are between 0 and
   * 1 (100%), but volumes lower than .2 are excluded as being too quiet.
   * The values are linear between .2 and 1.0.
   * @return Array<{value: number, label: string}>
   * @private
   */
  speechVolumeTicks_: function() {
    return Array.from(Array(9).keys()).map(x => {
      return this.initTick_(x * .1 + .2);
    });
  },

  /**
   * Initializes i18n labels for ticks arrays.
   * @param {number} tick The value to make a tick for.
   * @return {{value: number, label: string}}
   * @private
   */
  initTick_: function(tick) {
    let value = (100 * tick).toFixed(0);
    let label = value === '100' ? this.i18n('defaultPercentage', value) :
                                  this.i18n('percentage', value);
    return {label: label, value: tick};
  },

  /**
   * Populates the list of languages and voices for the UI to use in display.
   * @param {Array<TtsHandlerVoice>} voices
   * @private
   */
  populateVoiceList_: function(voices) {
    // Build a map of language code to human-readable language and voice.
    let result = {};
    voices.forEach(voice => {
      if (!result[voice.languageCode]) {
        result[voice.languageCode] = {
          language: voice.displayLanguage,
          code: voice.languageCode,
          voices: []
        };
      }
      // Each voice gets a unique ID from its name and extension.
      voice.id =
          JSON.stringify({name: voice.name, extension: voice.extensionId});
      // TODO(katie): Make voices a map rather than an array to enforce
      // uniqueness, then convert back to an array for polymer repeat.
      result[voice.languageCode].voices.push(voice);
    });
    this.set('languagesToVoices', Object.values(result));
  },

  /**
   * Sets the list of Text-to-Speech extensions for the UI.
   * @param {Array<TtsHandlerExtension>} extensions
   * @private
   */
  populateExtensionList_: function(extensions) {
    this.extensions = extensions;
  },

  /**
   * A function used for sorting languages alphabetically.
   * @param {Object} first A languageToVoices array item.
   * @param {Object} second A languageToVoices array item.
   * @return {number} The result of the comparison.
   * @private
   */
  alphabeticalSort_: function(first, second) {
    return first.language.localeCompare(second.language);
  },

  /**
   * Tests whether a language has just once voice.
   * @param {Object} lang A languageToVoices array item.
   * @return {boolean} True if the item has only one voice.
   * @private
   */
  hasOneLanguage_: function(lang) {
    return lang['voices'].length == 1;
  },

  /**
   * Returns a list of objects that can be used as drop-down menu options for a
   * language. This is a list of voices in that language.
   * @param {Object} lang A languageToVoices array item.
   * @return {Array<Object>} An array of menu options with a value and name.
   * @private
   */
  menuOptionsForLang_: function(lang) {
    return lang.voices.map(voice => {
      return {value: voice.id, name: voice.name};
    });
  },

  /** @private */
  onPreviewTtsClick_: function() {
    let utter = new window.SpeechSynthesisUtterance();
    if (!utter)
      return;
    utter.text = this.$.previewInput.value;
    window.speechSynthesis.speak(utter);
  },

});
