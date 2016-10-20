// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox options page.
 *
 */

goog.provide('cvox.OptionsPage');

goog.require('Msgs');
goog.require('cvox.BrailleTable');
goog.require('cvox.BrailleTranslatorManager');
goog.require('cvox.ChromeEarcons');
goog.require('cvox.ChromeTts');
goog.require('cvox.ChromeVox');
goog.require('cvox.ChromeVoxPrefs');
goog.require('cvox.CommandStore');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.KeyMap');
goog.require('cvox.KeySequence');
goog.require('cvox.PlatformFilter');
goog.require('cvox.PlatformUtil');

/**
 * Class to manage the options page.
 * @constructor
 */
cvox.OptionsPage = function() {
};

/**
 * The ChromeVoxPrefs object.
 * @type {cvox.ChromeVoxPrefs}
 */
cvox.OptionsPage.prefs;


/**
 * A mapping from keycodes to their human readable text equivalents.
 * This is initialized in cvox.OptionsPage.init for internationalization.
 * @type {Object<string>}
 */
cvox.OptionsPage.KEYCODE_TO_TEXT = {
};

/**
 * A mapping from human readable text to keycode values.
 * This is initialized in cvox.OptionsPage.init for internationalization.
 * @type {Object<string>}
 */
cvox.OptionsPage.TEXT_TO_KEYCODE = {
};

/**
 * Initialize the options page by setting the current value of all prefs,
 * building the key bindings table, and adding event listeners.
 * @suppress {missingProperties} Property prefs never defined on Window
 */
cvox.OptionsPage.init = function() {
  cvox.OptionsPage.prefs = chrome.extension.getBackgroundPage().prefs;
  cvox.OptionsPage.populateKeyMapSelect();
  cvox.OptionsPage.populateVoicesSelect();
  cvox.BrailleTable.getAll(function(tables) {
    /** @type {!Array<cvox.BrailleTable.Table>} */
    cvox.OptionsPage.brailleTables = tables;
    cvox.OptionsPage.populateBrailleTablesSelect();
  });
  chrome.storage.local.get({'brailleWordWrap': true}, function(items) {
    $('brailleWordWrap').checked = items.brailleWordWrap;
  });

  Msgs.addTranslatedMessagesToDom(document);
  cvox.OptionsPage.hidePlatformSpecifics();

  cvox.OptionsPage.update();

  document.addEventListener('change', cvox.OptionsPage.eventListener, false);
  document.addEventListener('click', cvox.OptionsPage.eventListener, false);
  document.addEventListener('keydown', cvox.OptionsPage.eventListener, false);

  cvox.ExtensionBridge.addMessageListener(function(message) {
    if (message['keyBindings'] || message['prefs']) {
      cvox.OptionsPage.update();
    }
  });

  $('selectKeys').addEventListener(
      'click', cvox.OptionsPage.reset, false);

  if (cvox.PlatformUtil.matchesPlatform(cvox.PlatformFilter.WML)) {
    $('version').textContent =
        chrome.app.getDetails().version;
  }
};

/**
 * Update the value of controls to match the current preferences.
 * This happens if the user presses a key in a tab that changes a
 * pref.
 */
cvox.OptionsPage.update = function() {
  var prefs = cvox.OptionsPage.prefs.getPrefs();
  for (var key in prefs) {
    // TODO(rshearer): 'active' is a pref, but there's no place in the
    // options page to specify whether you want ChromeVox active.
    var elements = document.querySelectorAll('*[name="' + key + '"]');
    for (var i = 0; i < elements.length; i++) {
      cvox.OptionsPage.setValue(elements[i], prefs[key]);
    }
  }
};

/**
 * Populate the keymap select element with stored keymaps
 */
cvox.OptionsPage.populateKeyMapSelect = function() {
  var select = $('cvox_keymaps');
  for (var id in cvox.KeyMap.AVAILABLE_MAP_INFO) {
    var info = cvox.KeyMap.AVAILABLE_MAP_INFO[id];
    var option = document.createElement('option');
    option.id = id;
    option.className = 'i18n';
    option.setAttribute('msgid', id);
    if (cvox.OptionsPage.prefs.getPrefs()['currentKeyMap'] == id) {
      option.setAttribute('selected', '');
    }
    select.appendChild(option);
  }

  select.addEventListener('change', cvox.OptionsPage.reset, true);
};

/**
 * Populates the voices select with options.
 */
cvox.OptionsPage.populateVoicesSelect = function() {
  var select = $('voices');

  function setVoiceList() {
    var selectedVoiceName =
        chrome.extension.getBackgroundPage()['getCurrentVoice']();
    chrome.tts.getVoices(function(voices) {
      select.innerHTML = '';
      // TODO(plundblad): voiceName can actually be omitted in the TTS engine.
      // We should generate a name in that case.
      voices.forEach(function(voice) {
        voice.voiceName = voice.voiceName || '';
      });
      voices.sort(function(a, b) {
        return a.voiceName.localeCompare(b.voiceName);
      });
      voices.forEach(function(voice) {
        var option = document.createElement('option');
        option.voiceName = voice.voiceName;
        option.innerText = option.voiceName;
        if (selectedVoiceName === voice.voiceName) {
          option.setAttribute('selected', '');
        }
        select.add(option);
      });
    });
  }

  window.speechSynthesis.onvoiceschanged = setVoiceList;
  setVoiceList();

  select.addEventListener('change', function(evt) {
    var selIndex = select.selectedIndex;
    var sel = select.options[selIndex];
    chrome.storage.local.set({voiceName: sel.voiceName});
  }, true);
};

/**
 * Populates the braille select control.
 */
cvox.OptionsPage.populateBrailleTablesSelect = function() {
  if (!cvox.ChromeVox.isChromeOS) {
    return;
  }
  var tables = cvox.OptionsPage.brailleTables;
  var populateSelect = function(node, dots) {
    var activeTable = localStorage[node.id] || localStorage['brailleTable'];
    // Gather the display names and sort them according to locale.
    var items = [];
    for (var i = 0, table; table = tables[i]; i++) {
      if (table.dots !== dots) {
        continue;
      }
      items.push({id: table.id,
                  name: cvox.BrailleTable.getDisplayName(table)});
    }
    items.sort(function(a, b) { return a.name.localeCompare(b.name);});
    for (var i = 0, item; item = items[i]; ++i) {
      var elem = document.createElement('option');
      elem.id = item.id;
      elem.textContent = item.name;
      if (item.id == activeTable) {
        elem.setAttribute('selected', '');
      }
      node.appendChild(elem);
    }
  };
  var select6 = $('brailleTable6');
  var select8 = $('brailleTable8');
  populateSelect(select6, '6');
  populateSelect(select8, '8');

  var handleBrailleSelect = function(node) {
    return function(evt) {
      var selIndex = node.selectedIndex;
      var sel = node.options[selIndex];
      localStorage['brailleTable'] = sel.id;
      localStorage[node.id] = sel.id;
      cvox.OptionsPage.getBrailleTranslatorManager().refresh();
    };
  };

  select6.addEventListener('change', handleBrailleSelect(select6), true);
  select8.addEventListener('change', handleBrailleSelect(select8), true);

  var tableTypeButton = $('brailleTableType');
  var updateTableType = function(setFocus) {
    var currentTableType = localStorage['brailleTableType'] || 'brailleTable6';
    if (currentTableType == 'brailleTable6') {
      select6.removeAttribute('aria-hidden');
      select6.setAttribute('tabIndex', 0);
      select6.style.display = 'block';
      if (setFocus) {
        select6.focus();
      }
      select8.setAttribute('aria-hidden', 'true');
      select8.setAttribute('tabIndex', -1);
      select8.style.display = 'none';
      localStorage['brailleTable'] = localStorage['brailleTable6'];
      localStorage['brailleTableType'] = 'brailleTable6';
      tableTypeButton.textContent =
          Msgs.getMsg('options_braille_table_type_6');
    } else {
      select6.setAttribute('aria-hidden', 'true');
      select6.setAttribute('tabIndex', -1);
      select6.style.display = 'none';
      select8.removeAttribute('aria-hidden');
      select8.setAttribute('tabIndex', 0);
      select8.style.display = 'block';
      if (setFocus) {
        select8.focus();
      }
      localStorage['brailleTable'] = localStorage['brailleTable8'];
      localStorage['brailleTableType'] = 'brailleTable8';
      tableTypeButton.textContent =
          Msgs.getMsg('options_braille_table_type_8');
    }
    cvox.OptionsPage.getBrailleTranslatorManager().refresh();
  };
  updateTableType(false);

  tableTypeButton.addEventListener('click', function(evt) {
    var oldTableType = localStorage['brailleTableType'];
    localStorage['brailleTableType'] =
        oldTableType == 'brailleTable6' ? 'brailleTable8' : 'brailleTable6';
    updateTableType(true);
  }, true);
};

/**
 * Set the html element for a preference to match the given value.
 * @param {Element} element The HTML control.
 * @param {string} value The new value.
 */
cvox.OptionsPage.setValue = function(element, value) {
  if (element.tagName == 'INPUT' && element.type == 'checkbox') {
    element.checked = (value == 'true');
  } else if (element.tagName == 'INPUT' && element.type == 'radio') {
    element.checked = (String(element.value) == value);
  } else {
    element.value = value;
  }
};

/**
 * Event listener, called when an event occurs in the page that might
 * affect one of the preference controls.
 * @param {Event} event The event.
 * @return {boolean} True if the default action should occur.
 */
cvox.OptionsPage.eventListener = function(event) {
  window.setTimeout(function() {
    var target = event.target;
    if (target.id == 'brailleWordWrap') {
      chrome.storage.local.set({brailleWordWrap: target.checked});
    } else if (target.classList.contains('pref')) {
      if (target.tagName == 'INPUT' && target.type == 'checkbox') {
        cvox.OptionsPage.prefs.setPref(target.name, target.checked);
      } else if (target.tagName == 'INPUT' && target.type == 'radio') {
        var key = target.name;
        var elements = document.querySelectorAll('*[name="' + key + '"]');
        for (var i = 0; i < elements.length; i++) {
          if (elements[i].checked) {
            cvox.OptionsPage.prefs.setPref(target.name, elements[i].value);
          }
        }
      }
    } else if (target.classList.contains('key')) {
      var keySeq = cvox.KeySequence.fromStr(target.value);
      var success = false;
      if (target.id == 'cvoxKey') {
        cvox.OptionsPage.prefs.setPref(target.id, target.value);
        cvox.OptionsPage.prefs.sendPrefsToAllTabs(true, true);
        success = true;
      } else {
        success =
            cvox.OptionsPage.prefs.setKey(target.id, keySeq);

        // TODO(dtseng): Don't surface conflicts until we have a better
        // workflow.
      }
    }
  }, 0);
  return true;
};

/**
 * Refreshes all dynamic content on the page.
 * This includes all key related information.
 */
cvox.OptionsPage.reset = function() {
  var selectKeyMap = $('cvox_keymaps');
  var id = selectKeyMap.options[selectKeyMap.selectedIndex].id;

  var msgs = Msgs;
  var announce = cvox.OptionsPage.prefs.getPrefs()['currentKeyMap'] == id ?
      msgs.getMsg('keymap_reset', [msgs.getMsg(id)]) :
      msgs.getMsg('keymap_switch', [msgs.getMsg(id)]);
  cvox.OptionsPage.updateStatus_(announce);

  cvox.OptionsPage.prefs.switchToKeyMap(id);
  Msgs.addTranslatedMessagesToDom(document);
};

/**
 * Updates the status live region.
 * @param {string} status The new status.
 * @private
 */
cvox.OptionsPage.updateStatus_ = function(status) {
  $('status').innerText = status;
};


/**
 * Hides all elements not matching the current platform.
 */
cvox.OptionsPage.hidePlatformSpecifics = function() {
  if (!cvox.ChromeVox.isChromeOS) {
    var elements = document.body.querySelectorAll('.chromeos');
    for (var i = 0, el; el = elements[i]; i++) {
      el.setAttribute('aria-hidden', 'true');
      el.style.display = 'none';
    }
  }
};


/**
 * @return {cvox.BrailleTranslatorManager}
 */
cvox.OptionsPage.getBrailleTranslatorManager = function() {
  return chrome.extension.getBackgroundPage()['braille_translator_manager'];
};

document.addEventListener('DOMContentLoaded', function() {
  cvox.OptionsPage.init();
}, false);
