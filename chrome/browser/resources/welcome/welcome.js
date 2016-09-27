// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('welcome', function() {
  'use strict';

  function onAccept(e) {
    chrome.send('handleActivateSignIn');
  }

  function onDecline(e) {
    chrome.send('handleUserDecline');
  }

  function initialize() {
    $('accept-button').addEventListener('click', onAccept);
    $('decline-button').addEventListener('click', onDecline);
  }

  return {
    initialize: initialize
  };
});

document.addEventListener('DOMContentLoaded', welcome.initialize);
