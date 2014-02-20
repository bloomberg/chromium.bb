// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Inline login UI.
 */

<include src="../gaia_auth_host/gaia_auth_host.js"></include>

cr.define('inline.login', function() {
  'use strict';

  /**
   * The auth extension host instance.
   * @type {Object}
   */
  var authExtHost;

  /**
   * Handler of auth host 'ready' event.
   */
  function onAuthReady() {
    $('contents').classList.toggle('loading', false);
  }

  /**
   * Handler of auth host 'completed' event.
   * @param {!Object} credentials Credentials of the completed authentication.
   */
  function onAuthCompleted(credentials) {
    chrome.send('completeLogin', [credentials]);
    $('contents').classList.toggle('loading', true);
  }

  /**
   * Initialize the UI.
   */
  function initialize() {
    authExtHost = new cr.login.GaiaAuthHost('signin-frame');
    authExtHost.addEventListener('ready', onAuthReady);

    chrome.send('initialize');
  }

  /**
   * Loads auth extension.
   * @param {Object} data Parameters for auth extension.
   */
  function loadAuthExtension(data) {
    authExtHost.load(data.authMode, data, onAuthCompleted);
    $('contents').classList.toggle('loading', true);
  }

  /**
   * Closes the inline login dialog.
   */
  function closeDialog() {
    chrome.send('DialogClose', ['']);
  }

  /**
   * Invoked when failed to get oauth2 refresh token.
   */
  function handleOAuth2TokenFailure() {
    // TODO(xiyuan): Show an error UI.
    authExtHost.reload();
    $('contents').classList.toggle('loading', true);
  }

  return {
    initialize: initialize,
    loadAuthExtension: loadAuthExtension,
    closeDialog: closeDialog,
    handleOAuth2TokenFailure: handleOAuth2TokenFailure
  };
});

document.addEventListener('DOMContentLoaded', inline.login.initialize);
