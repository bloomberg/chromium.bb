// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 */
let SelectToSpeakOptionsPage = function() {
  this.init_();
};

SelectToSpeakOptionsPage.prototype = {
  /**
   * Translate the page and sync all of the control values to the
   * values loaded from chrome.storage.
   */
  init_: function() {
    this.addTranslatedMessagesToDom_();
    this.populateVoiceList_('voice');
    window.speechSynthesis.onvoiceschanged = (function() {
      this.populateVoiceList_('voice');
    }.bind(this));
    this.syncSelectControlToPref_('voice', 'voice');
    this.syncSelectControlToPref_('rate', 'rate');
    this.syncCheckboxControlToPref_('wordHighlight', 'wordHighlight');
  },

  /**
   * Processes an HTML DOM, replacing text content with translated text messages
   * on elements marked up for translation.  Elements whose class attributes
   * contain the 'i18n' class name are expected to also have an msgid
   * attribute. The value of the msgid attributes are looked up as message
   * IDs and the resulting text is used as the text content of the elements.
   * @private
   */
  addTranslatedMessagesToDom_: function() {
    var elts = document.querySelectorAll('.i18n');
    for (var i = 0; i < elts.length; i++) {
      var msgid = elts[i].getAttribute('msgid');
      if (!msgid) {
        throw new Error('Element has no msgid attribute: ' + elts[i]);
      }
      var translated = chrome.i18n.getMessage('select_to_speak_' + msgid);
      if (elts[i].tagName == 'INPUT') {
        elts[i].setAttribute('placeholder', translated);
      } else {
        elts[i].textContent = translated;
      }
      elts[i].classList.add('i18n-processed');
    }
  },

  /**
   * Populate a select element with the list of TTS voices.
   * @param {string} selectId The id of the select element.
   * @private
   */
  populateVoiceList_: function(selectId) {
    chrome.tts.getVoices(function(voices) {
      var select = document.getElementById(selectId);
      select.innerHTML = '';
      var option = document.createElement('option');
      option.voiceName = null;
      option.innerText = option.voiceName;

      voices.forEach(function(voice) {
        voice.voiceName = voice.voiceName || '';
      });
      voices.sort(function(a, b) {
        return a.voiceName.localeCompare(b.voiceName);
      });
      voices.forEach(function(voice) {
        if (!voice.voiceName)
          return;
        var option = document.createElement('option');
        option.voiceName = voice.voiceName;
        option.innerText = option.voiceName;
        select.add(option);
      });
      if (select.updateFunction) {
        select.updateFunction();
      }
    });
  },

  /**
   * Populate a checkbox with its current setting.
   * @param {string} checkboxId The id of the checkbox element.
   * @private
   */
  syncCheckboxControlToPref_: function(checkboxId, pref) {
    let checkbox = document.getElementById(checkboxId);

    function updateFromPref() {
      chrome.storage.sync.get(pref, function(items) {
        let value = items[pref];
        if (value != null) {
          checkbox.checked = value;
        }
      });
    }

    checkbox.addEventListener('change', function() {
      let setParams = {};
      setParams[pref] = checkbox.checked;
      chrome.storage.sync.set(setParams);
    });

    checkbox.updateFunction = updateFromPref;
    updateFromPref();
    chrome.storage.onChanged.addListener(updateFromPref);
  },

  /**
   * Given the id of an HTML select element and the name of a chrome.storage
   * pref, sync them both ways.
   * @param {string} selectId The id of the select element.
   * @param {string} pref The name of a chrome.storage pref.
   */
  syncSelectControlToPref_: function(selectId, pref) {
    var element = document.getElementById(selectId);

    function updateFromPref() {
      chrome.storage.sync.get(pref, function(items) {
        var value = items[pref];
        element.selectedIndex = -1;
        for (var i = 0; i < element.options.length; ++i) {
          if (element.options[i].value == value) {
            element.selectedIndex = i;
            break;
          }
        }
      });
    }

    element.addEventListener('change', function() {
      var newValue = element.options[element.selectedIndex].value;
      var setParams = {};
      setParams[pref] = newValue;
      chrome.storage.sync.set(setParams);
    });

    element.updateFunction = updateFromPref;
    updateFromPref();
    chrome.storage.onChanged.addListener(updateFromPref);
  }
};

new SelectToSpeakOptionsPage();
