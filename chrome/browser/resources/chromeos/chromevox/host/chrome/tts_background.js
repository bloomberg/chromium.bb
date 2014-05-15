// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Text-To-Speech commands to Chrome's native TTS
 * extension API.
 *
 */

goog.provide('cvox.TtsBackground');

goog.require('cvox.AbstractTts');
goog.require('cvox.ChromeTtsBase');
goog.require('cvox.ChromeVox');
goog.require('cvox.MathMap');
goog.require('goog.i18n.MessageFormat');

/**
 * @constructor
 * @param {boolean=} opt_enableMath Whether to process math. Used when running
 * on forge. Defaults to true.
 * @extends {cvox.ChromeTtsBase}
 */
cvox.TtsBackground = function(opt_enableMath) {
  opt_enableMath = opt_enableMath == undefined ? true : opt_enableMath;
  goog.base(this);
  this.currentVoice = localStorage['voiceName'] || '';

  this.ttsProperties['rate'] = (parseFloat(localStorage['rate']) ||
      this.propertyDefault['rate']);
  this.ttsProperties['pitch'] = (parseFloat(localStorage['pitch']) ||
      this.propertyDefault['pitch']);
  this.ttsProperties['volume'] = (parseFloat(localStorage['volume']) ||
      this.propertyDefault['volume']);

  // Use the current locale as the speech language if not otherwise
  // specified.
  if (this.ttsProperties['lang'] == undefined) {
    this.ttsProperties['lang'] =
        chrome.i18n.getMessage('@@ui_locale').replace('_', '-');
  }

  this.lastEventType = 'end';

  this.setPreferredVoice_();
  if (!this.currentVoice) {
    this.setDefaultVoiceName_();
  }

  /** @private {number} */
  this.currentPunctuationEcho_ =
      parseInt(localStorage[cvox.AbstractTts.PUNCTUATION_ECHO] || 1, 10);

  /**
   * @type {!Array.<{name:(string),
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
      regexp: /[$#"*<>\\\/\{\}+=~`%]/g,
      clear: false
    },

    /**
     * Punctuation echoed for the 'all' option.
     */
    {
      name: 'all',
      msg: 'all_punctuation',
      regexp: /[-$#"()*;:<>\n\\\/\{\}\[\]+='~`!@_.,?%]/g,
      clear: false
    }
  ];

  /**
   * A list of punctuation characters that should always be spliced into output
   * even with literal word substitutions.
   * This is important for tts prosity.
   * @type {!Array.<string>}
   * @private
  */
  this.retainPunctuation_ =
      [';', '?', '!', '\''];

  /**
   * Mapping for math elements.
   * @type {cvox.MathMap}
   */
  this.mathmap = opt_enableMath ? new cvox.MathMap() : null;

  /**
   * The id of a callback returned from setTimeout.
   * @type {number|undefined}
   */
  this.timeoutId_;

  try {
    /**
     * @type {Object.<string, string>}
     * @private
     * @const
     */
    this.PHONETIC_MAP_ = /** @type {Object.<string, string>} */(
        JSON.parse(cvox.ChromeVox.msgs.getMsg('phonetic_map')));
  } catch (e) {
    console.log('Error; unable to parse phonetic map msg.');
  }

  /**
   * Capturing tts event listeners.
   * @type {Array.<cvox.TtsCapturingEventListener>}
   * @private
   */
  this.capturingTtsEventListeners_ = [];

  /** @type {boolean} @private */
  this.doNotInterrupt_ = false;
};
goog.inherits(cvox.TtsBackground, cvox.ChromeTtsBase);


/**
 * The amount of time to wait before speaking a phonetic word for a
 * letter.
 * @type {number}
 * @private
 * @const
 */
cvox.TtsBackground.PHONETIC_DELAY_MS_ = 1000;

/**
 * Sets the current voice to the one that the user selected on the options page
 * if that voice exists.
 * @private
 */
cvox.TtsBackground.prototype.setPreferredVoice_ = function() {
  var self = this;
  chrome.tts.getVoices(
      function(voices) {
        for (var i = 0, v; v = voices[i]; i++) {
          if (v['voiceName'] == localStorage['voiceName']) {
            self.currentVoice = v['voiceName'];
            return;
          }
        }
      });
};


/** @override */
cvox.TtsBackground.prototype.speak = function(
    textString, queueMode, properties) {
  goog.base(this, 'speak', textString, queueMode, properties);

  if (!properties) {
    properties = {};
  }

  // Create a snapshot of the callbacks to use since there is a possibility that
  // we will be overwriting properties[callbacks] in the splitTextString loop
  // later on. Without the snapshot, if the properties[callbacks] are
  // overwritten, the callbacks will not be called by 'onEvent' - this then
  // results in a memory leak as calling the callbacks is what deletes their
  // associated function in the functionmap on the content script side of
  // cvox.ChromeTts.
  var startCallback = properties['startCallback'];
  var endCallback = properties['endCallback'];

  // Chunk to improve responsiveness. Use a replace/split pattern in order to
  // retain the original punctuation.
  var splitTextString = textString.replace(/([-\n\r.,!?;])(\s)/g, '$1$2|');
  splitTextString = splitTextString.split('|');
  // Since we are substituting the chunk delimiters back into the string, only
  // recurse when there are more than 2 split items. This should result in only
  // one recursive call.
  if (splitTextString.length > 2) {
    for (var i = 0; i < splitTextString.length; i++) {
      properties['startCallback'] = i == 0 ? startCallback : null;
      properties['endCallback'] =
          i == (splitTextString.length - 1) ? endCallback : null;
      this.speak(splitTextString[i], queueMode, properties);
      queueMode = cvox.AbstractTts.QUEUE_MODE_QUEUE;
    }
    return this;
  }

  textString = this.preprocess(textString, properties);

  // TODO(dtseng): Google TTS has bad performance when speaking numbers. This
  // pattern causes ChromeVox to read numbers as digits rather than words.
  textString = this.getNumberAsDigits_(textString);

  // TODO(dtseng): Google TTS flushes the queue when encountering strings of
  // this pattern which stops ChromeVox speech.
  if (!textString || !textString.match(/\w+/g)) {
    // We still want to callback for listeners in our content script.
    if (startCallback) {
      startCallback();
    }
    if (endCallback) {
      endCallback();
    }
    if (queueMode === cvox.AbstractTts.QUEUE_MODE_FLUSH) {
      this.stop();
    }
    return this;
  }

  this.doNotInterrupt_ =
      goog.isDef(properties['doNotInterrupt']) ? properties['doNotInterrupt'] :
          this.doNotInterrupt_;

  var mergedProperties = this.mergeProperties(properties);
  mergedProperties['enqueue'] = this.doNotInterrupt_ ||
      (queueMode === cvox.AbstractTts.QUEUE_MODE_QUEUE);

  if (this.currentVoice && (this.currentVoice == localStorage['voiceName'])) {
    mergedProperties['voiceName'] = this.currentVoice;
  }
  if (localStorage['voiceName'] &&
      this.currentVoice != localStorage['voiceName']) {
    this.setPreferredVoice_();
  }

  mergedProperties['onEvent'] = goog.bind(function(event) {
    this.lastEventType = event['type'];

    switch (event.type) {
      case 'start':
        this.capturingTtsEventListeners_.forEach(function(listener) {
          listener.onTtsStart();
        });
        if (startCallback) {
          startCallback();
        }
        break;
      case 'end':
        this.capturingTtsEventListeners_.forEach(function(listener) {
          listener.onTtsEnd();
        });
      // Intentionally falls through.
      case 'interrupted':
        if (endCallback) {
          endCallback();
        }
        this.doNotInterrupt_ = false;
        break;
      case 'error':
        this.onError_(event['errorMessage']);
        break;
    }
  }, this);
  chrome.tts.speak(textString, mergedProperties);
};

/** @override */
cvox.TtsBackground.prototype.increaseOrDecreaseProperty =
    function(propertyName, increase) {
      goog.base(this, 'increaseOrDecreaseProperty', propertyName, increase);
  localStorage[propertyName] = this.ttsProperties[propertyName];
};

/** @override */
cvox.TtsBackground.prototype.isSpeaking = function() {
  goog.base(this, 'isSpeaking');
  return this.lastEventType != 'end';
};

/** @override */
cvox.TtsBackground.prototype.stop = function() {
  goog.base(this, 'stop');
  chrome.tts.stop();
};

/** @override */
cvox.TtsBackground.prototype.addCapturingEventListener = function(listener) {
  this.capturingTtsEventListeners_.push(listener);
};

/**
 * An error handler passed as a callback to chrome.tts.speak.
 * @param {string} errorMessage Describes the error (set by onEvent).
 * @private
 */
cvox.TtsBackground.prototype.onError_ = function(errorMessage) {
  // Reset voice related parameters.
  delete localStorage['voiceName'];
};

/**
 * Converts an engine property value to a percentage from 0.00 to 1.00.
 * @return {?number} The percentage of the property.
 */
cvox.TtsBackground.prototype.propertyToPercentage = function(property) {
  return (this.ttsProperties[property] - this.propertyMin[property]) /
         Math.abs(this.propertyMax[property] - this.propertyMin[property]);
};


/**
 * @override
 */
cvox.TtsBackground.prototype.preprocess = function(text, properties) {
  properties = properties ? properties : {};

  // Perform specialized processing, such as mathematics.
  if (properties.math) {
    text = this.preprocessMath_(text, properties.math);
  }

  // Perform generic processing.
  text = goog.base(this, 'preprocess', text, properties);

  // Perform any remaining processing such as punctuation expansion.
  var pE = null;
  if (properties[cvox.AbstractTts.PUNCTUATION_ECHO]) {
    for (var i = 0; pE = this.punctuationEchoes_[i]; i++) {
      if (properties[cvox.AbstractTts.PUNCTUATION_ECHO] == pE.name) {
        break;
      }
    }
  } else {
    pE = this.punctuationEchoes_[this.currentPunctuationEcho_];
  }
  text =
      text.replace(pE.regexp, this.createPunctuationReplace_(pE.clear));

  // Try pronouncing phonetically for single characters. Cancel previous calls
  // to pronouncePhonetically_ if we fail to pronounce on this invokation or if
  // this text is math which should never be pronounced phonetically.
  if (properties.math ||
      !properties['phoneticCharacters'] ||
      !this.pronouncePhonetically_(text)) {
    this.clearTimeout_();
  }

  // Try looking up in our unicode tables for a short description.
  if (!properties.math && text.length == 1 && this.mathmap) {
    text = this.mathmap.store.lookupString(
        text.toLowerCase(),
        cvox.MathStore.createDynamicConstraint('default', 'short')) || text;
  }

  //  Remove all whitespace from the beginning and end, and collapse all
  // inner strings of whitespace to a single space.
  text = text.replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '');

  return text;
};


/**
 * Method that cycles among the available punctuation echo levels.
 * @return {string} The resulting punctuation level message id.
 */
cvox.TtsBackground.prototype.cyclePunctuationEcho = function() {
  this.currentPunctuationEcho_ =
      (this.currentPunctuationEcho_ + 1) % this.punctuationEchoes_.length;
  localStorage[cvox.AbstractTts.PUNCTUATION_ECHO] =
      this.currentPunctuationEcho_;
  return this.punctuationEchoes_[this.currentPunctuationEcho_].msg;
};


/**
 * Process a math expression into a string suitable for a speech engine.
 * @param {string} text Text representing a math expression.
 * @param {Object= } math Parameter containing information how to
 *     process the math expression.
 * @return {string} The string with a spoken version of the math expression.
 * @private
 */
cvox.TtsBackground.prototype.preprocessMath_ = function(text, math) {
  if (!this.mathmap) {
    return text;
  }
  var result = '';
  var dynamicCstr = cvox.MathStore.createDynamicConstraint(
      math['domain'], math['style']);
  result = this.mathmap.store.lookupString(text, dynamicCstr);
  if (result) {
    return result;
  }
  return text;
};


/**
 * Converts a number into space-separated digits.
 * For numbers containing 4 or fewer digits, we return the original number.
 * This ensures that numbers like 123,456 or 2011 are not "digitized" while
 * 123456 is.
 * @param {string} text The text to process.
 * @return {string} A string with all numbers converted.
 * @private
 */
cvox.TtsBackground.prototype.getNumberAsDigits_ = function(text) {
  return text.replace(/\d+/g, function(num) {
    if (num.length <= 4) {
      return num;
    }
    return num.split('').join(' ');
  });
};


/**
 * Constructs a function for string.replace that handles description of a
 *  punctuation character.
 * @param {boolean} clear Whether we want to use whitespace in place of match.
 * @return {function(string): string} The replacement function.
 * @private
 */
cvox.TtsBackground.prototype.createPunctuationReplace_ = function(clear) {
  return goog.bind(function(match) {
    var retain = this.retainPunctuation_.indexOf(match) != -1 ?
        match : ' ';
    return clear ? retain :
        ' ' + (new goog.i18n.MessageFormat(cvox.ChromeVox.msgs.getMsg(
                cvox.AbstractTts.CHARACTER_DICTIONARY[match])))
            .format({'COUNT': 1}) + retain + ' ';
  }, this);
};


/**
 * Pronounces single letters phonetically after some timeout.
 * @param {string} text The text.
 * @return {boolean} True if the text resulted in speech.
 * @private
 */
cvox.TtsBackground.prototype.pronouncePhonetically_ = function(text) {
  if (!this.PHONETIC_MAP_) {
    return false;
  }
  text = text.toLowerCase();
  text = this.PHONETIC_MAP_[text];
  if (text) {
    this.clearTimeout_();
    var self = this;
    this.timeoutId_ = setTimeout(function() {
      self.speak(text, 1);
    }, cvox.TtsBackground.PHONETIC_DELAY_MS_);
    return true;
  }
  return false;
};


/**
 * Clears the last timeout set via setTimeout.
 * @private
 */
cvox.TtsBackground.prototype.clearTimeout_ = function() {
  if (goog.isDef(this.timeoutId_)) {
    clearTimeout(this.timeoutId_);
    this.timeoutId_ = undefined;
  }
};

/**
 * Sets the name of a voice appropriate for the current locale preferring
 * non-remote voices.
 * @private
 */
cvox.TtsBackground.prototype.setDefaultVoiceName_ = function() {
  chrome.tts.getVoices(
      goog.bind(function(voices) {
        var currentLocale =
            chrome.i18n.getMessage('@@ui_locale').replace('_', '-');
        voices.sort(function(v1, v2) {
          if (v1['remote'] && !v2['remote']) {
            return 1;
          }
          if (!v1['remote'] && v2['remote']) {
            return -1;
          }
          if (v1['lang'] == currentLocale && v2['lang'] != currentLocale) {
            return -1;
          }
          if (v1['lang'] != currentLocale && v2['lang'] == currentLocale) {
            return 1;
          }
          return 0;
        });
        if (voices[0]) {
          var voiceName = voices[0].voiceName;
          this.currentVoice = voiceName;
        }
      }, this));
};
