/**
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.  Use of this
 * source code is governed by a BSD-style license that can be found in the
 * LICENSE file.
 */

//Contains true if multiple calendar option is checked, false otherwise.
var isMultiCalendar;

//adding listener when body is loaded to call init function.
window.addEventListener('load', init, false);

/**
 * Sets the value of multiple calendar checkbox based on value from
 * local storage.
 */
 function init() {
  isMultiCalendar = JSON.parse(localStorage.multiCalendar || false);
  $('multiCalendar').checked = isMultiCalendar;
  $('multiCalendarText').innerHTML =
      chrome.i18n.getMessage('multiCalendarText');
  $('multiCalendar').addEventListener('click', save);
  $('optionsTitle').innerHTML = chrome.i18n.getMessage('optionsTitle');
  $('imageTooltip').title = chrome.i18n.getMessage('imageTooltip');
  $('imageTooltip').alt = chrome.i18n.getMessage('imageTooltip');
  $('multiCalendarText').title = chrome.i18n.getMessage('multiCalendarToolTip');
  $('multiCalendar').title = chrome.i18n.getMessage('multiCalendarToolTip');
  $('extensionName').innerHTML = chrome.i18n.getMessage('extensionName');
  if (chrome.i18n.getMessage('direction') == 'rtl') {
    $('body').style.direction = 'rtl';
  }
};

/**
 * Saves the value of the checkbox into local storage.
 */
function save() {
  var multiCalendarId = $('multiCalendar');
  localStorage.multiCalendar = multiCalendarId.checked;
  if (multiCalendarId) {
    multiCalendar.disabled = true;
  }
  $('status').innerHTML = chrome.i18n.getMessage('status_saving');
  $('status').style.display = 'block';

  // Sends message to the background page notifying it that the settings
  // have updated.
  chrome.runtime.getBackgroundPage(function(bg) {
    bg.onSettingsChange();
    statusSaved();
  });
};

function statusSaved() {
  if ($('multiCalendar')) {
    if ($('multiCalendar').disabled) {
      $('status').innerHTML = chrome.i18n.getMessage('status_saved');
      $('status').style.display = 'block';
      setTimeout(
          function() { $('status').style.display = 'none'; }
          , 1500);
    }
    $('multiCalendar').disabled = false;
  }
};
