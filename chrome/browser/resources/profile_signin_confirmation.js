// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('profile_signin_confirmation', function() {
  'use strict';

  function initialize() {
    var args = JSON.parse(chrome.getVariableValue('dialogArguments'));
    $('dialog-message').textContent = loadTimeData.getStringF(
        'dialogMessage', args.username);
    $('dialog-prompt').textContent = loadTimeData.getStringF(
        'dialogPrompt', args.username);
    $('create-button').addEventListener('click', function() {
      chrome.send('createNewProfile');
    });
    $('continue-button').addEventListener('click', function() {
      chrome.send('continue');
    });
    $('cancel-button').addEventListener('click', function() {
      chrome.send('cancel');
    });
  }

  return {
    initialize: initialize
  };
});

document.addEventListener('DOMContentLoaded',
                          profile_signin_confirmation.initialize);
