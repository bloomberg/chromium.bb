// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Inline login UI.
 */

cr.define('inline.login', function() {
  'use strict';

  /**
   * The auth extension host instance.
   * @type {cr.login.GaiaAuthHost}
   */
  let authExtHost;

  /**
   * Whether the auth ready event has been fired, for testing purpose.
   */
  let authReadyFired;

  /**
   * Whether the login UI is loaded for signing in primary account.
   */
  let isLoginPrimaryAccount;

  function onResize(e) {
    chrome.send('switchToFullTab', [e.detail]);
  }

  function onAuthReady(e) {
    $('contents').classList.toggle('loading', false);
    authReadyFired = true;
    if (isLoginPrimaryAccount) {
      chrome.send('metricsHandler:recordAction', ['Signin_SigninPage_Shown']);
    }
  }

  function onDropLink(e) {
    // Navigate to the dropped link.
    window.location.href = e.detail;
  }

  function onNewWindow(e) {
    window.open(e.detail.targetUrl, '_blank');
    e.detail.window.discard();
  }

  function onAuthCompleted(e) {
    completeLogin(e.detail);
  }

  function completeLogin(credentials) {
    chrome.send('completeLogin', [credentials]);
    $('contents').classList.toggle('loading', true);
  }

  function onShowIncognito() {
    chrome.send('showIncognito');
  }

  /**
   * Initialize the UI.
   */
  function initialize() {
    $('navigation-button').addEventListener('click', navigationButtonClicked);
    authExtHost = new cr.login.GaiaAuthHost('signin-frame');
    authExtHost.addEventListener('dropLink', onDropLink);
    authExtHost.addEventListener('ready', onAuthReady);
    authExtHost.addEventListener('newWindow', onNewWindow);
    authExtHost.addEventListener('resize', onResize);
    authExtHost.addEventListener('authCompleted', onAuthCompleted);
    authExtHost.addEventListener('showIncognito', onShowIncognito);
    chrome.send('initialize');
  }

  /**
   * Loads auth extension.
   * @param {Object} data Parameters for auth extension.
   */
  function loadAuthExtension(data) {
    // TODO(rogerta): in when using webview, the |completeLogin| argument
    // is ignored.  See addEventListener() call above.
    authExtHost.load(data.authMode, data, completeLogin);
    $('contents')
        .classList.toggle(
            'loading',
            data.authMode != cr.login.GaiaAuthHost.AuthMode.DESKTOP ||
                data.constrained == '1');
    isLoginPrimaryAccount = data.isLoginPrimaryAccount;
  }

  /**
   * Closes the inline login dialog.
   */
  function closeDialog() {
    chrome.send('dialogClose', ['']);
  }

  /**
   * Sends a message 'lstFetchResults'. This is a specific message  sent when
   * the inline signin is loaded with reason REASON_FETCH_LST_ONLY. Handlers of
   * this message would expect a single argument a base::Dictionary value that
   * contains the values fetched from the gaia sign in endpoint.
   * @param {string} arg The string representation of the json data returned by
   *    the sign in dialog after it has finished the sign in process.
   */
  function sendLSTFetchResults(arg) {
    chrome.send('lstFetchResults', [arg]);
  }

  /**
   * Invoked when failed to get oauth2 refresh token.
   */
  function handleOAuth2TokenFailure() {
    // TODO(xiyuan): Show an error UI.
    authExtHost.reload();
    $('contents').classList.toggle('loading', true);
  }

  /**
   * Returns the auth host instance, for testing purpose.
   */
  function getAuthExtHost() {
    return authExtHost;
  }

  /**
   * Returns whether the auth UI is ready, for testing purpose.
   */
  function isAuthReady() {
    return authReadyFired;
  }

  function showBackButton() {
    $('navigation-button').ironIcon =
        isRTL() ? 'cr:arrow-forward' : 'cr:arrow-back';

    $('navigation-button')
        .setAttribute(
            'aria-label', loadTimeData.getString('accessibleBackButtonLabel'));
  }

  function showCloseButton() {
    $('navigation-button').ironIcon = 'cr:close';
    $('navigation-button').classList.add('enabled');
    $('navigation-button')
        .setAttribute(
            'aria-label', loadTimeData.getString('accessibleCloseButtonLabel'));
  }

  function navigationButtonClicked() {
    chrome.send('navigationButtonClicked');
  }

  return {
    closeDialog: closeDialog,
    sendLSTFetchResults: sendLSTFetchResults,
    getAuthExtHost: getAuthExtHost,
    handleOAuth2TokenFailure: handleOAuth2TokenFailure,
    initialize: initialize,
    isAuthReady: isAuthReady,
    loadAuthExtension: loadAuthExtension,
    navigationButtonClicked: navigationButtonClicked,
    showBackButton: showBackButton,
    showCloseButton: showCloseButton
  };
});

document.addEventListener('DOMContentLoaded', inline.login.initialize);
