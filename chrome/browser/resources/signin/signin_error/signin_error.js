/* Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

cr.define('signin.error', function() {
  'use strict';

  function initialize() {
    document.addEventListener('keydown', onKeyDown);
    $('confirmButton').addEventListener('click', onConfirm);
    $('closeButton').addEventListener('click', onConfirm);
    $('switchButton').addEventListener('click', onSwitchToExistingProfile);
    $('learnMoreLink').addEventListener('click', onLearnMore);
    if (loadTimeData.getBoolean('isSystemProfile')) {
      $('learnMoreLink').hidden = true;
    }
    chrome.send('initializedWithSize', [document.body.scrollHeight]);
  }

  function onKeyDown(e) {
    // If the currently focused element isn't something that performs an action
    // on "enter" being pressed and the user hits "enter", perform the default
    // action of the dialog, which is "OK".
    if (e.key == 'Enter' &&
        !/^(A|PAPER-BUTTON)$/.test(document.activeElement.tagName)) {
      $('confirmButton').click();
      e.preventDefault();
    }
  }

  function onConfirm(e) {
    chrome.send('confirm');
  }

  function onSwitchToExistingProfile(e) {
    chrome.send('switchToExistingProfile');
  }

  function onLearnMore(e) {
    chrome.send('learnMore');
  }

  function clearFocus() {
    document.activeElement.blur();
  }

  function removeSwitchButton() {
    $('switchButton').hidden = true;
    $('closeButton').hidden = true;
    $('confirmButton').hidden = false;
  }

  return {
    initialize: initialize,
    clearFocus: clearFocus,
    removeSwitchButton: removeSwitchButton
  };
});

document.addEventListener('DOMContentLoaded', signin.error.initialize);
