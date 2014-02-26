// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
<include src="../login/screen.js"></include>
<include src="../login/bubble.js"></include>
<include src="../login/display_manager.js"></include>
<include src="control_bar.js"></include>
<include src="../login/screen_account_picker.js"></include>
<include src="../login/user_pod_row.js"></include>
<include src="../login/resource_loader.js"></include>

cr.define('cr.ui', function() {
  var DisplayManager = cr.ui.login.DisplayManager;

  /**
  * Constructs an Out of box controller. It manages initialization of screens,
  * transitions, error messages display.
  * @extends {DisplayManager}
  * @constructor
  */
  function Oobe() {
  }

  cr.addSingletonGetter(Oobe);

  Oobe.prototype = {
    __proto__: DisplayManager.prototype,
  };

  /**
   * Shows the given screen.
   * @param {Object} screen Screen params dict, e.g. {id: screenId, data: data}
   */
  Oobe.showUserManagerScreen = function() {
    Oobe.getInstance().showScreen({id: 'account-picker',
                                   data: {disableAddUser: false}});
    // The ChromeOS account-picker will hide the AddUser button if a user is
    // logged in and the screen is "locked", so we must re-enabled it
    $('add-user-header-bar-item').hidden = false;
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  Oobe.launchUser = function(email, displayName) {
    chrome.send('launchUser', [email, displayName]);
  };

  /**
   * Disables signin UI.
   */
  Oobe.disableSigninUI = function() {
    DisplayManager.disableSigninUI();
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  Oobe.showSigninUI = function(opt_email) {
    DisplayManager.showSigninUI(opt_email);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attemps tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  Oobe.showSignInError = function(loginAttempts, message, link, helpId) {
    DisplayManager.showSignInError(loginAttempts, message, link, helpId);
  };

  /**
   * Clears error bubble as well as optional menus that could be open.
   */
  Oobe.clearErrors = function() {
    DisplayManager.clearErrors();
  };

  /**
   * Clears password field in user-pod.
   */
  Oobe.clearUserPodPassword = function() {
    DisplayManager.clearUserPodPassword();
  };

  /**
   * Restores input focus to currently selected pod.
   */
  Oobe.refocusCurrentPod = function() {
    DisplayManager.refocusCurrentPod();
  };

  // Export
  return {
    Oobe: Oobe
  };
});

cr.define('UserManager', function() {
  'use strict';

  function initialize() {
    cr.ui.login.DisplayManager.initialize();
    login.AccountPickerScreen.register();
    cr.ui.Bubble.decorate($('bubble'));
    login.HeaderBar.decorate($('login-header-bar'));
    chrome.send('userManagerInitialize');
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize
  };
});

var Oobe = cr.ui.Oobe;

// Allow selection events on components with editable text (password field)
// bug (http://code.google.com/p/chromium/issues/detail?id=125863)
disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
         src instanceof HTMLInputElement &&
         /text|password|search/.test(src.type);
});

document.addEventListener('DOMContentLoaded', UserManager.initialize);
