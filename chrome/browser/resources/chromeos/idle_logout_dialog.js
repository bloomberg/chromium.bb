// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings;

// Timer that will check and update the countdown every second.
var countdownTimer;
// Time at which we should logout the user unless we've been closed.
var logoutTime; // ms

/**
 * Decrements the countdown timer and updates the display on the dialog.
 */
function decrementTimer() {
  var currentTime = Date.now();
  if (logoutTime > currentTime) {
    secondsToRestart = Math.round((logoutTime - currentTime) / 1000);
    $('warning').innerHTML = localStrings.getStringF('warning',
                                                     secondsToRestart);
  } else {
    clearInterval(countdownTimer);
    chrome.send('requestLogout');
  }
}

/**
 * Starts the countdown to logout.
 */
function startCountdown(seconds) {
  logoutTime = Date.now() + seconds * 1000;
  $('warning').innerHTML = localStrings.getStringF('warning',
                                                   seconds);
  countdownTimer = setInterval(decrementTimer, 1000);
}

/**
 * Inserts translated strings on loading.
 */
function initialize() {
  localStrings = new LocalStrings();

  i18nTemplate.process(document, templateData);
  chrome.send('requestCountdown');
}

document.addEventListener('DOMContentLoaded', initialize);
