// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Text-To-Speech commands to Chrome's native TTS
 * extension API.
 *
 */

goog.provide('TtsBackground');

goog.require('PanelCommand');
goog.require('PhoneticData');
goog.require('AbstractTts');
goog.require('ChromeTtsBase');
goog.require('ChromeVox');
goog.require('goog.i18n.MessageFormat');

goog.require('constants');

const Utterance = class {
  /**
   * @param {string} textString The string of text to be spoken.
   * @param {Object} properties Speech properties to use for this utterance.
   */
  constructor(textString, properties) {
    this.textString = textString;
    this.properties = properties;
    this.id = Utterance.nextUtteranceId_++;
  }
};

/**
 * The next utterance id to use.
 * @type {number}
 * @private
 */
Utterance.nextUtteranceId_ = 1;

TtsBackground = class extends ChromeTtsBase {
  constructor() {
    super();

    this.lastEventType = 'end';

    /** @private {number} */
    this.currentPunctuationEcho_ =
        parseInt(localStorage[AbstractTts.PUNCTUATION_ECHO] || 1, 10);

    /**
     * @type {!Array<{name:(string),
     * msg:(string),
     * regexp:(RegExp),
     * clear:(boolean)}>}
     * @private
     */
    this.punctuationEchoes_ = [
      /**
       * Punctuation echoed for the 'none' option.
       */
      {
        name: 'none',
        msg: 'no_punctuation',
        regexp: /[-$#"()*;:<>\n\\\/+='~`@_]/g,
        clear: true
      },

      /**
       * Punctuation echoed for the 'some' option.
       */
      {
        name: 'some',
        msg: 'some_punctuation',
        regexp: /[$#"*<>\\\/\{\}+=~`%\u2022]/g,
        clear: false
      },

      /**
       * Punctuation echoed for the 'all' option.
       */
      {
        name: 'all',
        msg: 'all_punctuation',
        regexp: /[-$#"()*;:<>\n\\\/\{\}\[\]+='~`!@_.,?%\u2022]/g,
        clear: false
      }
    ];

    /**
     * A list of punctuation characters that should always be spliced into
     * output even with literal word substitutions. This is important for tts
     * prosity.
     * @type {!Array<string>}
     * @private
     */
    this.retainPunctuation_ = [';', '?', '!', '\''];

    /**
     * The id of a callback returned by setTimeout.
     * @type {number|undefined}
     */
    this.timeoutId_;

    /**
     * Capturing tts event listeners.
     * @type {Array<TtsCapturingEventListener>}
     * @private
     */
    this.capturingTtsEventListeners_ = [];

    /**
     * The current utterance.
     * @type {Utterance}
     * @private
     */
    this.currentUtterance_ = null;

    /**
     * The utterance queue.
     * @type {Array<Utterance>}
     * @private
     */
    this.utteranceQueue_ = [];

    /**
     * The current voice name.
     * @type {string}
     */
    this.currentVoice;

    // TODO(dtseng): Done while migrating away from using localStorage.
    if (localStorage['voiceName']) {
      chrome.storage.local.set({voiceName: localStorage['voiceName']});
      delete localStorage['voiceName'];
    }

    if (window.speechSynthesis) {
      window.speechSynthesis.onvoiceschanged = function() {
        chrome.storage.local.get({voiceName: ''}, function(items) {
          this.updateVoice_(items.voiceName);
        }.bind(this));
      }.bind(this);
    } else {
      // SpeechSynthesis API is not available on chromecast. Call
      // updateVoice_ to set the one and only voice as the current
      // voice.
      this.updateVoice_('');
    }

    chrome.storage.onChanged.addListener(function(changes, namespace) {
      if (changes.voiceName) {
        this.updateVoice_(changes.voiceName.newValue);
      }
    }.bind(this));

    // Migration: localStorage tts properties -> Chrome pref settings.
    if (localStorage['rate']) {
      chrome.settingsPrivate.setPref(
          'settings.tts.speech_rate', parseFloat(localStorage['rate']));
      delete localStorage['rate'];
    }
    if (localStorage['pitch']) {
      chrome.settingsPrivate.setPref(
          'settings.tts.speech_pitch', parseFloat(localStorage['pitch']));
      delete localStorage['pitch'];
    }
    if (localStorage['volume']) {
      chrome.settingsPrivate.setPref(
          'settings.tts.speech_volume', parseFloat(localStorage['volume']));
      delete localStorage['volume'];
    }

    // At startup.
    chrome.settingsPrivate.getAllPrefs(this.updateFromPrefs_.bind(this, false));

    // At runtime.
    chrome.settingsPrivate.onPrefsChanged.addListener(
        this.updateFromPrefs_.bind(this, true));
  }

  /** @override */
  speak(textString, queueMode, properties) {
    super.speak(textString, queueMode, properties);

    // |textString| gets manipulated throughout this function. Save the original
    // value for functions that may need it.
    const originalTextString = textString;

    if (this.ttsProperties[AbstractTts.VOLUME] === 0) {
      return this;
    }

    if (!properties) {
      properties = {};
    }

    if (textString.length > constants.OBJECT_MAX_CHARCOUNT) {
      // The text is too long. Try to split the text into multiple chunks based
      // on line breaks.
      this.speakSplittingText_(textString, queueMode, properties);
      return this;
    }

    textString = this.preprocess(textString, properties);

    // This pref on localStorage gets set by the options page.
    if (localStorage['numberReadingStyle'] == 'asDigits') {
      textString = this.getNumberAsDigits_(textString);
    }

    // TODO(dtseng): some TTS engines don't handle strings that don't produce
    // any speech very well. Handle empty and whitespace only strings (including
    // non-breaking space) here to mitigate the issue somewhat.
    if (TtsBackground.SKIP_WHITESPACE_.test(textString)) {
      // Explicitly call start and end callbacks before skipping this text.
      if (properties['startCallback']) {
        try {
          properties['startCallback']();
        } catch (e) {
        }
      }
      if (properties['endCallback']) {
        try {
          properties['endCallback']();
        } catch (e) {
        }
      }
      if (queueMode === QueueMode.FLUSH) {
        this.stop();
      }
      return this;
    }

    const mergedProperties = this.mergeProperties(properties);

    if (this.currentVoice && this.currentVoice !== constants.SYSTEM_VOICE) {
      mergedProperties['voiceName'] = this.currentVoice;
    }

    if (queueMode == QueueMode.CATEGORY_FLUSH &&
        !mergedProperties['category']) {
      queueMode = QueueMode.FLUSH;
    }

    const utterance = new Utterance(textString, mergedProperties);
    this.speakUsingQueue_(utterance, queueMode);
    // Attempt to queue phonetic speech with property['delay']. This ensures
    // that phonetic hints are delayed when we process them.
    this.pronouncePhonetically_(originalTextString, properties);
    return this;
  }

  /**
   * Split the given textString into smaller chunks and call this.speak() for
   * each chunks.
   * @param {string} textString The string of text to be spoken.
   * @param {QueueMode} queueMode The queue mode to use for speaking.
   * @param {Object=} properties Speech properties to use for this utterance.
   * @private
   */
  speakSplittingText_(textString, queueMode, properties) {
    const chunks = TtsBackground.splitUntilSmall(textString, '\n\r ');
    for (const chunk of chunks) {
      this.speak(chunk, queueMode, properties);
      queueMode = QueueMode.QUEUE;
    }
  }

  /**
   * Splits |text| until each substring's length is smaller than or equal to
   * constants.OBJECT_MAX_CHARCOUNT.
   * @param {string} text
   * @param {string} delimiters
   * @return {!Array<string>}
   */
  static splitUntilSmall(text, delimiters) {
    if (text.length == 0) {
      return [];
    }

    if (text.length <= constants.OBJECT_MAX_CHARCOUNT) {
      return [text];
    }

    const midIndex = text.length / 2;
    if (!delimiters) {
      return TtsBackground
          .splitUntilSmall(text.substring(0, midIndex), delimiters)
          .concat(TtsBackground.splitUntilSmall(
              text.substring(midIndex, text.length), delimiters));
    }

    const delimiter = delimiters[0];
    let splitIndex = text.lastIndexOf(delimiter, midIndex);
    if (splitIndex == -1) {
      splitIndex = text.indexOf(delimiter, midIndex);
    }

    if (splitIndex == -1) {
      delimiters = delimiters.slice(1);
      return TtsBackground.splitUntilSmall(text, delimiters);
    }

    return TtsBackground
        .splitUntilSmall(text.substring(0, splitIndex), delimiters)
        .concat(TtsBackground.splitUntilSmall(
            text.substring(splitIndex + 1, text.length), delimiters));
  }

  /**
   * Use the speech queue to handle the given speech request.
   * @param {Utterance} utterance The utterance to speak.
   * @param {QueueMode} queueMode The queue mode.
   * @private
   */
  speakUsingQueue_(utterance, queueMode) {
    // First, take care of removing the current utterance and flushing
    // anything from the queue we need to. If we remove the current utterance,
    // make a note that we're going to stop speech.
    if (queueMode == QueueMode.FLUSH || queueMode == QueueMode.CATEGORY_FLUSH) {
      (new PanelCommand(PanelCommandType.CLEAR_SPEECH)).send();

      if (this.shouldCancel_(this.currentUtterance_, utterance, queueMode)) {
        // Clear timeout in case currentUtterance_ is a delayed utterance.
        this.clearTimeout_();
        this.cancelUtterance_(this.currentUtterance_);
        this.currentUtterance_ = null;
      }
      let i = 0;
      while (i < this.utteranceQueue_.length) {
        if (this.shouldCancel_(this.utteranceQueue_[i], utterance, queueMode)) {
          this.cancelUtterance_(this.utteranceQueue_[i]);
          this.utteranceQueue_.splice(i, 1);
        } else {
          i++;
        }
      }
    }

    // Next, add the new utterance to the queue.
    this.utteranceQueue_.push(utterance);

    // Now start speaking the next item in the queue.
    this.startSpeakingNextItemInQueue_();
  }

  /**
   * If nothing is speaking, pop the first item off the speech queue and
   * start speaking it. This is called when a speech request is made and
   * when the current utterance finishes speaking.
   * @private
   */
  startSpeakingNextItemInQueue_() {
    if (this.currentUtterance_) {
      return;
    }

    if (this.utteranceQueue_.length == 0) {
      return;
    }

    // There is no voice to speak with (e.g. the tts system has not fully
    // initialized).
    if (!this.currentVoice) {
      return;
    }

    // Clear timeout for delayed utterances (hints and phonetic speech).
    this.clearTimeout_();

    // Check top of utteranceQueue for delayed utterance.
    if (this.utteranceQueue_[0].properties['delay']) {
      // Remove 'delay' property and set a timeout to process this utterance
      // after the delay has passed.
      delete this.utteranceQueue_[0].properties['delay'];
      this.timeoutId_ = setTimeout(
          () => this.startSpeakingNextItemInQueue_(),
          TtsBackground.hint_delay_ms_);

      return;
    }

    this.currentUtterance_ = this.utteranceQueue_.shift();
    const utterance = this.currentUtterance_;
    const utteranceId = utterance.id;

    utterance.properties['onEvent'] = goog.bind(function(event) {
      this.onTtsEvent_(event, utteranceId);
    }, this);

    const validatedProperties = {};
    for (let i = 0; i < TtsBackground.ALLOWED_PROPERTIES_.length; i++) {
      const p = TtsBackground.ALLOWED_PROPERTIES_[i];
      if (utterance.properties[p]) {
        validatedProperties[p] = utterance.properties[p];
      }
    }

    // Update the caption panel.
    if (utterance.properties && utterance.properties['pitch'] &&
        utterance.properties['pitch'] < this.ttsProperties['pitch']) {
      (new PanelCommand(
           PanelCommandType.ADD_ANNOTATION_SPEECH, utterance.textString))
          .send();
    } else {
      (new PanelCommand(
           PanelCommandType.ADD_NORMAL_SPEECH, utterance.textString))
          .send();
    }

    chrome.tts.speak(utterance.textString, validatedProperties);
  }

  /**
   * Called when we get a speech event from Chrome. We ignore any event
   * that doesn't pertain to the current utterance, but when speech starts
   * or ends we optionally call callback functions, and start speaking the
   * next utterance if there's another one enqueued.
   * @param {Object} event The TTS event from chrome.
   * @param {number} utteranceId The id of the associated utterance.
   * @private
   */
  onTtsEvent_(event, utteranceId) {
    this.lastEventType = event['type'];

    // Ignore events sent on utterances other than the current one.
    if (!this.currentUtterance_ || utteranceId != this.currentUtterance_.id) {
      return;
    }

    const utterance = this.currentUtterance_;

    switch (event.type) {
      case 'start':
        this.capturingTtsEventListeners_.forEach(function(listener) {
          listener.onTtsStart();
        });
        if (utterance.properties['startCallback']) {
          try {
            utterance.properties['startCallback']();
          } catch (e) {
          }
        }
        break;
      case 'end':
        // End callbacks could cause additional speech to queue up.
        this.currentUtterance_ = null;
        this.capturingTtsEventListeners_.forEach(function(listener) {
          listener.onTtsEnd();
        });
        if (utterance.properties['endCallback']) {
          try {
            utterance.properties['endCallback']();
          } catch (e) {
          }
        }
        this.startSpeakingNextItemInQueue_();
        break;
      case 'interrupted':
        this.cancelUtterance_(utterance);
        this.currentUtterance_ = null;
        for (let i = 0; i < this.utteranceQueue_.length; i++) {
          this.cancelUtterance_(this.utteranceQueue_[i]);
        }
        this.utteranceQueue_.length = 0;
        this.capturingTtsEventListeners_.forEach(function(listener) {
          listener.onTtsInterrupted();
        });
        break;
      case 'error':
        this.onError_(event['errorMessage']);
        this.startSpeakingNextItemInQueue_();
        break;
    }
  }

  /**
   * Determines if |utteranceToCancel| should be canceled (interrupted if
   * currently speaking, or removed from the queue if not), given the new
   * utterance we want to speak and the queue mode. If the queue mode is
   * QUEUE or FLUSH, the logic is straightforward. If the queue mode is
   * CATEGORY_FLUSH, we only flush utterances with the same category.
   *
   * @param {Utterance} utteranceToCancel The utterance in question.
   * @param {Utterance} newUtterance The new utterance we're enqueueing.
   * @param {QueueMode} queueMode The queue mode.
   * @return {boolean} True if this utterance should be canceled.
   * @private
   */
  shouldCancel_(utteranceToCancel, newUtterance, queueMode) {
    if (!utteranceToCancel) {
      return false;
    }
    if (utteranceToCancel.properties['doNotInterrupt']) {
      return false;
    }
    switch (queueMode) {
      case QueueMode.QUEUE:
        return false;
      case QueueMode.FLUSH:
        return true;
      case QueueMode.CATEGORY_FLUSH:
        return (
            utteranceToCancel.properties['category'] ==
            newUtterance.properties['category']);
    }
    return false;
  }

  /**
   * Do any cleanup necessary to cancel an utterance, like callings its
   * callback function if any.
   * @param {Utterance} utterance The utterance to cancel.
   * @private
   */
  cancelUtterance_(utterance) {
    if (utterance && utterance.properties['endCallback']) {
      try {
        utterance.properties['endCallback'](true);
      } catch (e) {
      }
    }
  }

  /** @override */
  increaseOrDecreaseProperty(propertyName, increase) {
    super.increaseOrDecreaseProperty(propertyName, increase);

    let pref;
    switch (propertyName) {
      case AbstractTts.RATE:
        pref = 'settings.tts.speech_rate';
        break;
      case AbstractTts.PITCH:
        pref = 'settings.tts.speech_pitch';
        break;
      case AbstractTts.VOLUME:
        pref = 'settings.tts.speech_volume';
        break;
      default:
        return;
    }
    const value = this.ttsProperties[propertyName];
    chrome.settingsPrivate.setPref(pref, value);
  }

  /** @override */
  isSpeaking() {
    super.isSpeaking();
    return this.lastEventType != 'end';
  }

  /** @override */
  stop() {
    super.stop();

    this.cancelUtterance_(this.currentUtterance_);
    this.currentUtterance_ = null;

    for (let i = 0; i < this.utteranceQueue_.length; i++) {
      this.cancelUtterance_(this.utteranceQueue_[i]);
    }

    this.utteranceQueue_.length = 0;

    (new PanelCommand(PanelCommandType.CLEAR_SPEECH)).send();
    chrome.tts.stop();

    this.capturingTtsEventListeners_.forEach(function(listener) {
      listener.onTtsInterrupted();
    });
  }

  /** @override */
  addCapturingEventListener(listener) {
    this.capturingTtsEventListeners_.push(listener);
  }

  /** @override */
  removeCapturingEventListener(listener) {
    this.capturingTtsEventListeners_ =
        this.capturingTtsEventListeners_.filter((item) => {
          return item != listener;
        });
  }

  /**
   * An error handler passed as a callback to chrome.tts.speak.
   * @param {string} errorMessage Describes the error (set by onEvent).
   * @private
   */
  onError_(errorMessage) {
    this.updateVoice_(this.currentVoice);
  }

  /**
   * @override
   */
  preprocess(text, properties) {
    properties = properties ? properties : {};

    // Perform generic processing.
    text = super.preprocess(text, properties);

    // Perform any remaining processing such as punctuation expansion.
    let pE = null;
    if (properties[AbstractTts.PUNCTUATION_ECHO]) {
      for (let i = 0; pE = this.punctuationEchoes_[i]; i++) {
        if (properties[AbstractTts.PUNCTUATION_ECHO] == pE.name) {
          break;
        }
      }
    } else {
      pE = this.punctuationEchoes_[this.currentPunctuationEcho_];
    }
    text = text.replace(pE.regexp, this.createPunctuationReplace_(pE.clear));

    // Remove all whitespace from the beginning and end, and collapse all
    // inner strings of whitespace to a single space.
    text = text.replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '');

    // Look for the pattern [number + lowercase letter], such as in "5g
    // network". We want to capitalize the letter to prevent it from being
    // substituted with a unit in the TTS engine; in the above case, the string
    // would get spoken as "5 grams network", which we want to avoid. We do not
    // match against the letter "a" in the regular expression because it is a
    // word and we do not want to capitalize it just because it comes after a
    // number.
    text = text.replace(
        /(\d)(\s*)([b-z])\b/g,
        (unused, num, whitespace, letter) =>
            num + whitespace + letter.toUpperCase());

    return text;
  }

  /** @override */
  toggleSpeechOnOrOff() {
    const previousValue = this.ttsProperties[AbstractTts.VOLUME];
    const toggle = function() {
      if (previousValue == 0) {
        this.ttsProperties[AbstractTts.VOLUME] = 1;
      } else {
        this.ttsProperties[AbstractTts.VOLUME] = 0;
      }
    }.bind(this);

    if (previousValue == 0) {
      toggle();
    } else {
      // Let the caller make any last minute announcements in the current call
      // stack.
      setTimeout(toggle, 0);
    }

    return previousValue == 0;
  }

  /**
   * Method that cycles among the available punctuation echo levels.
   * @return {string} The resulting punctuation level message id.
   */
  cyclePunctuationEcho() {
    this.currentPunctuationEcho_ =
        (this.currentPunctuationEcho_ + 1) % this.punctuationEchoes_.length;
    localStorage[AbstractTts.PUNCTUATION_ECHO] = this.currentPunctuationEcho_;
    return this.punctuationEchoes_[this.currentPunctuationEcho_].msg;
  }

  /**
   * Converts a number into space-separated digits.
   * For numbers containing 4 or fewer digits, we return the original number.
   * This ensures that numbers like 123,456 or 2011 are not "digitized" while
   * 123456 is.
   * @param {string} text The text to process.
   * @return {string} A string with all numbers converted.
   * @private
   */
  getNumberAsDigits_(text) {
    return text.replace(/\d+/g, function(num) {
      return num.split('').join(' ');
    });
  }

  /**
   * Constructs a function for string.replace that handles description of a
   *  punctuation character.
   * @param {boolean} clear Whether we want to use whitespace in place of match.
   * @return {function(string): string} The replacement function.
   * @private
   */
  createPunctuationReplace_(clear) {
    return goog.bind(function(match) {
      const retain = this.retainPunctuation_.indexOf(match) != -1 ? match : ' ';
      return clear ? retain :
                     ' ' +
              (new goog.i18n.MessageFormat(
                   Msgs.getMsg(AbstractTts.CHARACTER_DICTIONARY[match])))
                  .format({'COUNT': 1}) +
              retain + ' ';
    }, this);
  }

  /**
   * Queues phonetic disambiguation for characters if disambiguation is found.
   * @param {string} text The text for which we want to get phonetic data.
   * @param {Object} properties Speech properties to use for this utterance.
   * @private
   */
  pronouncePhonetically_(text, properties) {
    // Math should never be pronounced phonetically.
    if (properties.math) {
      return;
    }

    // Only pronounce phonetic hints when explicitly requested.
    if (!properties.phoneticCharacters) {
      return;
    }

    text = text.toLowerCase();
    // If undefined language, use the UI language of the browser as a best
    // guess.
    if (!properties['lang']) {
      properties['lang'] = chrome.i18n.getUILanguage();
    }

    const phoneticText = PhoneticData.forCharacter(text, properties['lang']);
    if (phoneticText) {
      properties['delay'] = true;
      this.speak(phoneticText, QueueMode.QUEUE, properties);
    }
  }

  /**
   * Clears the last timeout set via setTimeout.
   * @private
   */
  clearTimeout_() {
    if (goog.isDef(this.timeoutId_)) {
      clearTimeout(this.timeoutId_);
      this.timeoutId_ = undefined;
    }
  }

  /**
   * Update the current voice used to speak based upon values in storage. If
   * that does not succeed, fallback to use system locale when picking a voice.
   * @param {string} voiceName Voice name to set.
   * @param {function(string) : void=} opt_callback Called when the voice is
   * determined.
   * @private
   */
  updateVoice_(voiceName, opt_callback) {
    chrome.tts.getVoices(goog.bind(function(voices) {
      const systemVoice = {voiceName: constants.SYSTEM_VOICE};
      voices.unshift(systemVoice);
      const newVoice = voices.find((v) => {
        return v.voiceName == voiceName;
      }) ||
          systemVoice;
      if (newVoice) {
        this.currentVoice = newVoice.voiceName;
        this.startSpeakingNextItemInQueue_();
      }
      if (opt_callback) {
        opt_callback(this.currentVoice);
      }
    }, this));
  }

  /**
   * @param {boolean} announce
   * @param {Array<chrome.settingsPrivate.PrefObject>} prefs
   * @private
   */
  updateFromPrefs_(announce, prefs) {
    prefs.forEach((pref) => {
      let msg;
      let propertyName;
      switch (pref.key) {
        case 'settings.tts.speech_rate':
          propertyName = AbstractTts.RATE;
          msg = 'announce_rate';
          this.setHintDelayMS(/** @type {number} */ (pref.value));
          break;
        case 'settings.tts.speech_pitch':
          propertyName = AbstractTts.PITCH;
          msg = 'announce_pitch';
          break;
        case 'settings.tts.speech_volume':
          propertyName = AbstractTts.VOLUME;
          msg = 'announce_volume';
          break;
        default:
          return;
      }

      this.ttsProperties[propertyName] = pref.value;

      if (!announce) {
        return;
      }

      const valueAsPercent =
          Math.round(this.propertyToPercentage(propertyName) * 100);
      const announcement = Msgs.getMsg(msg, [valueAsPercent]);
      ChromeVox.tts.speak(
          announcement, QueueMode.FLUSH, AbstractTts.PERSONALITY_ANNOTATION);
    });
  }

  /** @override */
  resetTextToSpeechSettings() {
    super.resetTextToSpeechSettings();

    const rate = ChromeVox.tts.getDefaultProperty('rate');
    const pitch = ChromeVox.tts.getDefaultProperty('pitch');
    const volume = ChromeVox.tts.getDefaultProperty('volume');
    chrome.settingsPrivate.setPref('settings.tts.speech_rate', rate);
    chrome.settingsPrivate.setPref('settings.tts.speech_pitch', pitch);
    chrome.settingsPrivate.setPref('settings.tts.speech_volume', volume);

    // Ensure this announcement doesn't get cut off by speech triggered by
    // updateFromPrefs_().
    // Copy properties from AbstractTts.PERSONALITY_ANNOTATION and add the
    // doNotInterrupt property.
    const speechProperties = {};
    const sourceProperties = AbstractTts.PERSONALITY_ANNOTATION || {};
    for (const [key, value] of Object.entries(sourceProperties)) {
      speechProperties[key] = value;
    }
    speechProperties['doNotInterrupt'] = true;

    ChromeVox.tts.speak(
        Msgs.getMsg('announce_tts_default_settings'), QueueMode.FLUSH,
        speechProperties);
  }

  /**
   * Sets |hint_delay_ms_| given the speech rate.
   * We want an inverse relationship between the speech rate and the hint delay;
   * the faster the speech rate, the shorter the delay should be.
   * Default speech rate (value of 1) should map to a delay of 1000 MS.
   * @param {number} rate
   */
  setHintDelayMS(rate) {
    TtsBackground.hint_delay_ms_ = 1000 / rate;
  }
};


/**
 * The amount of time, in milliseconds, to wait before speaking a hint.
 * @type {number}
 * @private
 */
TtsBackground.hint_delay_ms_ = 1000;

/**
 * The list of properties allowed to be passed to the chrome.tts.speak API.
 * Anything outside this list will be stripped.
 * @type {Array<string>}
 * @private
 * @const
 */
TtsBackground.ALLOWED_PROPERTIES_ = [
  'desiredEventTypes', 'enqueue', 'extensionId', 'gender', 'lang', 'onEvent',
  'pitch', 'rate', 'requiredEventTypes', 'voiceName', 'volume'
];


/** @private {RegExp} */
TtsBackground.SKIP_WHITESPACE_ = /^[\s\u00a0]*$/;
