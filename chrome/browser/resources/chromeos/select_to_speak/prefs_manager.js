// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Manages getting and storing user preferences.
 * @constructor
 */
let PrefsManager = function() {
  /** @private {?string} */
  this.voiceNameFromPrefs_ = null;

  /** @private {?string} */
  this.voiceNameFromLocale_ = null;

  /** @private {Set<string>} */
  this.validVoiceNames_ = new Set();

  /** @private {number} */
  this.speechRate_ = 1.0;

  /** @private {number} */
  this.speechPitch_ = 1.0;

  /** @private {boolean} */
  this.wordHighlight_ = true;

  /** @const {string} */
  this.color_ = '#f73a98';

  /** @private {string} */
  this.highlightColor_ = '#5e9bff';

  /**
   * Get the list of TTS voices, and set the default voice if not already set.
   * @private
   */
  this.updateDefaultVoice_ = function() {
    var uiLocale = chrome.i18n.getMessage('@@ui_locale');
    uiLocale = uiLocale.replace('_', '-').toLowerCase();

    chrome.tts.getVoices((voices) => {
      this.validVoiceNames_ = new Set();

      if (voices.length == 0)
        return;

      voices.forEach((voice) => {
        if (!voice.eventTypes.includes('start') ||
            !voice.eventTypes.includes('end') ||
            !voice.eventTypes.includes('word') ||
            !voice.eventTypes.includes('cancelled')) {
          return;
        }
        this.validVoiceNames_.add(voice.voiceName);
      });

      voices.sort(function(a, b) {
        function score(voice) {
          if (voice.lang === undefined)
            return -1;
          var lang = voice.lang.toLowerCase();
          var s = 0;
          if (lang == uiLocale)
            s += 2;
          if (lang.substr(0, 2) == uiLocale.substr(0, 2))
            s += 1;
          return s;
        }
        return score(b) - score(a);
      });

      this.voiceNameFromLocale_ = voices[0].voiceName;

      chrome.storage.sync.get(['voice'], (prefs) => {
        if (!prefs['voice'])
          chrome.storage.sync.set({'voice': PrefsManager.SYSTEM_VOICE});
      });
    });
  };
};

/**
 * Constant representing the system TTS voice.
 * @type {string}
 * @public
 */
PrefsManager.SYSTEM_VOICE = 'select_to_speak_system_voice';

/**
 * Loads preferences from chrome.storage, sets default values if
 * necessary, and registers a listener to update prefs when they
 * change.
 * @public
 */
PrefsManager.prototype.initPreferences = function() {
  var updatePrefs = () => {
    chrome.storage.sync.get(
        ['voice', 'rate', 'pitch', 'wordHighlight', 'highlightColor'],
        (prefs) => {
          if (prefs['voice']) {
            this.voiceNameFromPrefs_ = prefs['voice'];
          }
          if (prefs['rate']) {
            this.speechRate_ = parseFloat(prefs['rate']);
          } else {
            chrome.storage.sync.set({'rate': this.speechRate_});
          }
          if (prefs['pitch']) {
            this.speechPitch_ = parseFloat(prefs['pitch']);
          } else {
            chrome.storage.sync.set({'pitch': this.speechPitch_});
          }
          if (prefs['wordHighlight'] !== undefined) {
            this.wordHighlight_ = prefs['wordHighlight'];
          } else {
            chrome.storage.sync.set({'wordHighlight': this.wordHighlight_});
          }
          if (prefs['highlightColor']) {
            this.highlightColor_ = prefs['highlightColor'];
          } else {
            chrome.storage.sync.set({'highlightColor': this.highlightColor_});
          }
        });
  };

  updatePrefs();
  chrome.storage.onChanged.addListener(updatePrefs);

  this.updateDefaultVoice_();
  window.speechSynthesis.onvoiceschanged = (function() {
                                             this.updateDefaultVoice_();
                                           }).bind(this);
};

/**
 * Generates the basic speech options for Select-to-Speak based on user
 * preferences. Call for each chrome.tts.speak.
 * @return {Object} options The TTS options.
 * @public
 */
PrefsManager.prototype.speechOptions = function() {
  let options = {
    rate: this.speechRate_,
    pitch: this.speechPitch_,
    enqueue: true
  };

  // To use the default (system) voice: don't specify options['voiceName'].
  if (this.voiceNameFromPrefs_ === PrefsManager.SYSTEM_VOICE)
    return options;

  // Pick the voice name from prefs first, or the one that matches
  // the locale next, but don't pick a voice that isn't currently
  // loaded. If no voices are found, leave the voiceName option
  // unset to let the browser try to route the speech request
  // anyway if possible.
  var valid = '';
  this.validVoiceNames_.forEach(function(voiceName) {
    if (valid)
      valid += ',';
    valid += voiceName;
  });
  if (this.voiceNameFromPrefs_ &&
      this.validVoiceNames_.has(this.voiceNameFromPrefs_)) {
    options['voiceName'] = this.voiceNameFromPrefs_;
  } else if (
      this.voiceNameFromLocale_ &&
      this.validVoiceNames_.has(this.voiceNameFromLocale_)) {
    options['voiceName'] = this.voiceNameFromLocale_;
  }
  return options;
};

/**
 * Gets the user's speech pitch preference.
 * @return {number} The user-selected speech pitch.
 * @public
 */
PrefsManager.prototype.speechPitch = function() {
  return this.speechPitch_;
};

/**
 * Gets the user's speech rate preference.
 * @return {number} The user-selected speech rate.
 * @public
 */
PrefsManager.prototype.speechRate = function() {
  return this.speechRate_;
};

/**
 * Gets the user's word highlighting enabled preference.
 * @return {boolean} True if word highlighting is enabled.
 * @public
 */
PrefsManager.prototype.wordHighlightingEnabled = function() {
  return this.wordHighlight_;
};

/**
 * Gets the user's word highlighting color preference.
 * @return {string} Highlight color.
 * @public
 */
PrefsManager.prototype.highlightColor = function() {
  return this.highlightColor_;
};

/**
 * Gets the focus ring color. This is not currently a user preference but it
 * could be in the future; stored here for similarity to highlight color.
 * @return {string} Highlight color.
 * @public
 */
PrefsManager.prototype.focusRingColor = function() {
  return this.color_;
};
