// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to let user change their IdP password along
 * with cryptohome password.
 */

cr.define('insession.password.change', function() {
  'use strict';

  let authExtHost;

  /**
   * Initialize the UI.
   */
  function initialize() {
    authExtHost = new cr.samlPasswordChange.Authenticator('signin-frame');
    authExtHost.addEventListener('authCompleted', onAuthCompleted_);
    chrome.send('initialize');
  }

  function onAuthCompleted_(e) {
    chrome.send(
        'changePassword', [e.detail.old_passwords, e.detail.new_passwords]);
  }

  /**
   * Loads auth extension.
   * @param {Object} data Parameters for auth extension.
   */
  function loadAuthExtension(data) {
    authExtHost.load(data);
  }

  return {
    initialize: initialize,
    loadAuthExtension: loadAuthExtension,
  };
});

document.addEventListener(
    'DOMContentLoaded', insession.password.change.initialize);
