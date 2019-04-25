// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox options page.
 *
 */

goog.provide('cvox.OptionsPage');

goog.require('BluetoothBrailleDisplayUI');
goog.require('ConsoleTts');
goog.require('EventStreamLogger');
goog.require('Msgs');
goog.require('PanelCommand');
goog.require('cvox.BrailleTable');
goog.require('cvox.BrailleTranslatorManager');
goog.require('cvox.ChromeEarcons');
goog.require('cvox.ChromeTts');
goog.require('cvox.ChromeVox');
goog.require('cvox.ChromeVoxPrefs');
goog.require('cvox.CommandStore');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.PlatformFilter');
goog.require('cvox.PlatformUtil');

/**
 * Class to manage the options page.
 * @constructor
 */
cvox.OptionsPage = function() {};

/**
 * The ChromeVoxPrefs object.
 * @type {cvox.ChromeVoxPrefs}
 */
cvox.OptionsPage.prefs;

/**
 * The ChromeVoxConsoleTts object.
 * @type {ConsoleTts}
 */
cvox.OptionsPage.consoleTts;

/**
 * Display style options for virtual braille display.
 * @enum {number}
 */
cvox.OptionsPage.DisplayStyle = {
  INTERLEAVE: 0,
  SIDE_BY_SIDE: 1
};

/**
 * Initialize the options page by setting the current value of all prefs, and
 * adding event listeners.
 * @suppress {missingProperties} Property prefs never defined on Window
 * @this {cvox.OptionsPage}
 */
cvox.OptionsPage.init = function() {
  cvox.OptionsPage.prefs = chrome.extension.getBackgroundPage().prefs;
  cvox.OptionsPage.consoleTts =
      chrome.extension.getBackgroundPage().ConsoleTts.getInstance();
  cvox.OptionsPage.populateVoicesSelect();
  cvox.BrailleTable.getAll(function(tables) {
    /** @type {!Array<cvox.BrailleTable.Table>} */
    cvox.OptionsPage.brailleTables = tables;
  });
  chrome.storage.local.get({'brailleWordWrap': true}, function(items) {
    $('braille-word-wrap').checked = items.brailleWordWrap;
  });

  chrome.storage.local.get({'virtualBrailleRows': 1}, function(items) {
    $('virtual-braille-display-rows-input').value = items['virtualBrailleRows'];
  });
  chrome.storage.local.get({'virtualBrailleColumns': 40}, function(items) {
    $('virtual-braille-display-columns-input').value =
        items['virtualBrailleColumns'];
  });
  var changeToInterleave =
      Msgs.getMsg('options_change_current_display_style_interleave');
  var changeToSideBySide =
      Msgs.getMsg('options_change_current_display_style_side_by_side');
  var currentlyDisplayingInterleave =
      Msgs.getMsg('options_current_display_style_interleave');
  var currentlyDisplayingSideBySide =
      Msgs.getMsg('options_current_display_style_side_by_side');

  var showEventStreamFilters = Msgs.getMsg('options_show_event_stream_filters');
  var hideEventStreamFilters = Msgs.getMsg('options_hide_event_stream_filters');
  $('toggle-event-stream-filters').textContent = showEventStreamFilters;
  cvox.OptionsPage.disableEventStreamFilterCheckBoxes(
      localStorage['enableEventStreamLogging'] == 'false');

  if (localStorage['audioStrategy']) {
    for (var i = 0, opt; opt = $('audio-strategy').options[i]; i++) {
      if (opt.id == localStorage['audioStrategy']) {
        opt.setAttribute('selected', '');
      }
    }
  }

  chrome.commandLinePrivate.hasSwitch(
      'enable-experimental-accessibility-chromevox-language-switching',
      function(enabled) {
        if (!enabled) {
          $('languageSwitchingOption').hidden = true;
        }
      });

  chrome.commandLinePrivate.hasSwitch(
      'enable-experimental-accessibility-chromevox-rich-text-indication',
      function(enabled) {
        if (!enabled) {
          $('richTextIndicationOption').style.display = 'none';
        }
      });

  var registerEventStreamFiltersListener = function() {
    $('toggle-event-stream-filters').addEventListener('click', function(evt) {
      if ($('event-stream-filters').hidden) {
        $('event-stream-filters').hidden = false;
        $('toggle-event-stream-filters').textContent = hideEventStreamFilters;
      } else {
        $('event-stream-filters').hidden = true;
        $('toggle-event-stream-filters').textContent = showEventStreamFilters;
      }
    });
  };

  var toggleShowDeveloperOptions = function() {
    $('developer-speech-logging').hidden =
        !$('developer-speech-logging').hidden;
    $('developer-earcon-logging').hidden =
        !$('developer-earcon-logging').hidden;
    $('developer-braille-logging').hidden =
        !$('developer-braille-logging').hidden;
    $('developer-event-stream').hidden = !$('developer-event-stream').hidden;
    $('show-developer-log').hidden = !$('show-developer-log').hidden;
    $('chromevox-developer-options-more').hidden =
        !($('chromevox-developer-options-more').hidden);
    $('chromevox-developer-options-less').hidden =
        !($('chromevox-developer-options-less').hidden);
  };

  var toggleBrailleSettings = function() {
    $('braille-settings-more').hidden = !($('braille-settings-more').hidden);
    $('braille-settings-less').hidden = !($('braille-settings-less').hidden);
    $('6-dot-braille').hidden = !($('6-dot-braille').hidden);
    $('8-dot-braille').hidden = !($('8-dot-braille').hidden);
    $('braille-word-wrap').hidden = !($('braille-word-wrap').hidden);
  };

  $('braille-settings-more').addEventListener('click', function(evt) {
    toggleBrailleSettings();
  });

  $('braille-settings-less').addEventListener('click', function(evt) {
    toggleBrailleSettings();
  });

  var toggleVirtualBrailleSettings = function() {
    $('virtual-braille-settings-more').hidden =
        !($('virtual-braille-settings-more').hidden);
    $('virtual-braille-settings-less').hidden =
        !($('virtual-braille-settings-less').hidden);
    $('virtual-braille-settings-num-lines').hidden =
        !($('virtual-braille-settings-num-lines').hidden);
    $('virtual-braille-settings-lines-cells').hidden =
        !($('virtual-braille-settings-lines-cells').hidden);
    $('virtual-braille-settings-display').hidden =
        !($('virtual-braille-settings-display').hidden);
  };

  $('virtual-braille-settings-more').addEventListener('click', function(evt) {
    toggleVirtualBrailleSettings();
  });

  $('virtual-braille-settings-less').addEventListener('click', function(evt) {
    toggleVirtualBrailleSettings();
  });

  $('braille-description-6').addEventListener('click', function(evt) {
    $('braille-description-6').checked = true;
    $('braille-description-8').checked = false;
  });
  $('braille-description-8').addEventListener('click', function(evt) {
    $('braille-description-8').checked = true;
    $('braille-description-6').checked = false;
  });

  /** @type {!BluetoothBrailleDisplayUI} */
  cvox.OptionsPage.bluetoothBrailleDisplayUI = new BluetoothBrailleDisplayUI();
  var bluetoothBraille = $('bluetooth-braille');
  if (bluetoothBraille) {
    cvox.OptionsPage.bluetoothBrailleDisplayUI.attach(bluetoothBraille);
  }

  var toggleBluetoothBrailleSettings = function() {
    $('bt-braille-settings-more').hidden =
        !($('bt-braille-settings-more').hidden);
    $('bt-braille-settings-less').hidden =
        !($('bt-braille-settings-less').hidden);
    $('bluetooth-braille').hidden = !($('bluetooth-braille').hidden);
  };

  $('bt-braille-settings-more').addEventListener('click', function(evt) {
    toggleBluetoothBrailleSettings();
  });

  $('bt-braille-settings-less').addEventListener('click', function(evt) {
    toggleBluetoothBrailleSettings();
  });

  $('chromevox-developer-options-more')
      .addEventListener('click', function(evt) {
        toggleShowDeveloperOptions();
      });

  $('chromevox-developer-options-less')
      .addEventListener('click', function(evt) {
        toggleShowDeveloperOptions();
      });

  $('open-developer-log').addEventListener('click', function(evt) {
    let logPage = {url: 'cvox2/background/log.html'};
    chrome.tabs.create(logPage);
  });

  // Hide developer options by default.
  $('developer-speech-logging').hidden = true;
  $('developer-earcon-logging').hidden = true;
  $('developer-braille-logging').hidden = true;
  $('developer-event-stream').hidden = true;
  $('show-developer-log').hidden = true;

  registerEventStreamFiltersListener();
  Msgs.addTranslatedMessagesToDom(document);
  cvox.OptionsPage.hidePlatformSpecifics();

  cvox.OptionsPage.update();

  document.addEventListener('change', cvox.OptionsPage.eventListener, false);
  document.addEventListener('click', cvox.OptionsPage.eventListener, false);
  document.addEventListener('keydown', cvox.OptionsPage.eventListener, false);

  window.addEventListener('storage', (event) => {
    if (event.key == 'speakTextUnderMouse') {
      chrome.accessibilityPrivate.enableChromeVoxMouseEvents(
          event.newValue == String(true));
    }
  });

  cvox.ExtensionBridge.addMessageListener(function(message) {
    if (message['prefs']) {
      cvox.OptionsPage.update();
    }
  });

  if (cvox.PlatformUtil.matchesPlatform(cvox.PlatformFilter.WML)) {
    $('version').textContent = chrome.app.getDetails().version;
  }

  var clearVirtualDisplay = function() {
    var groups = [];
    var sizeOfDisplay =
        parseInt($('virtual-braille-display-rows-input').innerHTML, 10) *
        parseInt($('virtual-braille-display-columns-input').innerHTML, 10);
    for (var i = 0; i < sizeOfDisplay; i++) {
      groups.push(['X', 'X']);
    }
    (new PanelCommand(PanelCommandType.UPDATE_BRAILLE, {
      groups: groups
    })).send();
  };

  handleNumericalInputPref(
      'virtual-braille-display-rows-input', 'virtualBrailleRows');
  handleNumericalInputPref(
      'virtual-braille-display-columns-input', 'virtualBrailleColumns');

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
 * Adds event listeners to input boxes to update local storage values and
 * make sure that the input is a positive nonempty number between 1 and 99.
 * @param {string} id Id of the input box.
 * @param {string} pref Preference key in localStorage to access and modify.
 */
var handleNumericalInputPref = function(id, pref) {
  $(id).addEventListener('input', function(evt) {
    if ($(id).value === '') {
      return;
    } else if (
        parseInt($(id).value, 10) < 1 || parseInt($(id).value, 10) > 99) {
      chrome.storage.local.get(pref, function(items) {
        $(id).value = items[pref];
      });
    } else {
      var items = {};
      items[pref] = $(id).value;
      chrome.storage.local.set(items);
    }
  }, true);

  $(id).addEventListener('focusout', function(evt) {
    if ($(id).value === '')
      chrome.storage.local.get(pref, function(items) {
        $(id).value = items[pref];
      });
  }, true);
};

/**
 * Populates the voices select with options.
 */
cvox.OptionsPage.populateVoicesSelect = function() {
  var select = $('voices');

  function setVoiceList() {
    var selectedVoice =
        chrome.extension.getBackgroundPage()['getCurrentVoice']();
    let addVoiceOption = (visibleVoiceName, voiceName) => {
      let option = document.createElement('option');
      option.voiceName = voiceName;
      option.innerText = visibleVoiceName;
      if (selectedVoice === voiceName) {
        option.setAttribute('selected', '');
      }
      select.add(option);
    };
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
      addVoiceOption(
          chrome.i18n.getMessage('chromevox_system_voice'),
          constants.SYSTEM_VOICE);
      voices.forEach((voice) => {
        addVoiceOption(voice.voiceName, voice.voiceName);
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
 * @param {string} tableType The braille table type (6 or 8 dot).
 */
cvox.OptionsPage.populateBrailleTablesSelect = function(tableType) {
  if (!cvox.ChromeVox.isChromeOS) {
    return;
  }
  localStorage['brailleTable'] = tableType;
  localStorage['brailleTableType'] = tableType;
  cvox.OptionsPage.getBrailleTranslatorManager().refresh(
      localStorage['brailleTable']);
  var tables = cvox.OptionsPage.brailleTables;
  var populateSelect = function(node, dots) {
    var activeTable = localStorage[node.id] || localStorage['brailleTable'];
    // Gather the display names and sort them according to locale.
    var items = [];
    for (var i = 0, table; table = tables[i]; i++) {
      if (table.dots !== dots) {
        continue;
      }
      items.push({id: table.id, name: cvox.BrailleTable.getDisplayName(table)});
    }
    items.sort(function(a, b) {
      return a.name.localeCompare(b.name);
    });
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
  var select6 = $('braille-table-6');
  var select8 = $('braille-table-8');
  populateSelect(select6, '6');
  populateSelect(select8, '8');

  var handleBrailleSelect = function(node) {
    return function(evt) {
      var selIndex = node.selectedIndex;
      var sel = node.options[selIndex];
      localStorage['brailleTable'] = sel.id;
      localStorage[node.id] = sel.id;
      cvox.OptionsPage.getBrailleTranslatorManager().refresh(
          localStorage['brailleTable']);
    };
  };
  if (tableType == 'braille-table-6') {
    handleBrailleSelect(select6);
  } else if (tableType == 'braille-table-8') {
    handleBrailleSelect(select8);
  }
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
 * Disable event stream logging filter check boxes.
 * Check boxes should be disabled when event stream logging is disabled.
 * @param {boolean} disable
 */
cvox.OptionsPage.disableEventStreamFilterCheckBoxes = function(disable) {
  var filters = document.querySelectorAll('.option-eventstream > input');
  for (var i = 0; i < filters.length; i++)
    filters[i].disabled = disable;
};

/**
 * Event listener, called when an event occurs in the page
 * that might affect one of the preference controls.
 * @param {Event} event The event.
 * @return {boolean} True if the default action should
 *     occur.
 */
cvox.OptionsPage.eventListener = function(event) {
  window.setTimeout(function() {
    var target = event.target;
    if (target.id == 'braille-word-wrap') {
      chrome.storage.local.set({brailleWordWrap: target.checked});
    } else if (
        target.id == 'braille-table-6' || target.id == 'braille-table-8') {
      cvox.OptionsPage.populateBrailleTablesSelect(target.id);
    } else if (target.id == 'change-display-style') {
      var currentIndex = target.selectedIndex;
      localStorage['brailleSideBySide'] =
          ((currentIndex == cvox.OptionsPage.DisplayStyle.SIDE_BY_SIDE) ?
               true :
               false);
      var groups = [];
      var sizeOfDisplay =
          parseInt($('virtual-braille-display-rows-input').innerHTML, 10) *
          parseInt($('virtual-braille-display-columns-input').innerHTML, 10);
      for (var i = 0; i < sizeOfDisplay; i++) {
        groups.push(['X', 'X']);
      }
      (new PanelCommand(PanelCommandType.UPDATE_BRAILLE, {
        groups: groups
      })).send();
    } else if (target.className.indexOf('logging') != -1) {
      cvox.OptionsPage.prefs.setLoggingPrefs(target.name, target.checked);
      if (target.name == 'enableEventStreamLogging')
        cvox.OptionsPage.disableEventStreamFilterCheckBoxes(!target.checked);
    } else if (target.className.indexOf('eventstream') != -1) {
      cvox.OptionsPage.prefs.setPref(target.name, target.checked);
      chrome.extension.getBackgroundPage()
          .EventStreamLogger.instance.notifyEventStreamFilterChanged(
              target.name, target.checked);
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
      } else if (target.id == 'audio-strategy') {
        var selIndex = target.selectedIndex;
        var sel = target.options[selIndex];
        var value = sel ? sel.id : 'audioNormal';
        cvox.OptionsPage.prefs.setPref(target.id, value);
      }
    }
  }, 0);
  return true;
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

window.addEventListener('beforeunload', function(e) {
  cvox.OptionsPage.bluetoothBrailleDisplayUI.detach();
});
