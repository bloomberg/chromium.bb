// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('lockScreen', function() {
  'use strict';

  /**
   * Initialize the lock screen by setting up buttons and substituting in
   * translated strings.
   */
  function initialize() {
    i18nTemplate.process(document, templateData);

    $('unlock').onclick = unlockScreen;
    $('signout-link').onclick = signout;
    $('user-image').src = 'chrome://userimage/' + templateData.email;
    $('password').focus();
    displayError('');
  }

  function displayError(message) {
    $('error-box').textContent = message;
    if (message) {
      $('error-box').className = 'visible';
    } else {
      $('error-box').className = '';
    }
  }

  /**
   * Attempt to unlock the screen with the current password.
   */
  function unlockScreen() {
    chrome.send('unlockScreenRequest', [$('password').value]);
  }

  /**
   * Request signing out the current user.
   */
  function signout() {
    chrome.send('signoutRequest');
  }

  return {
    initialize: initialize,
    displayError: displayError,
  };
});

document.addEventListener('DOMContentLoaded', lockScreen.initialize);
