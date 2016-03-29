// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../../../../ui/login/screen.js">
<include src="../../../../ui/login/bubble.js">
<include src="../../../../ui/login/login_ui_tools.js">
<include src="../../../../ui/login/display_manager.js">
<include src="../../../../ui/login/account_picker/screen_account_picker.js">
<include src="../../../../ui/login/account_picker/user_pod_row.js">


cr.define('cr.ui', function() {
  var DisplayManager = cr.ui.login.DisplayManager;

  /**
   * Manages initialization of screens, transitions, and error messages.
   * @constructor
   * @extends {DisplayManager}
  */
  function UserManager() {}

  cr.addSingletonGetter(UserManager);

  UserManager.prototype = {
    __proto__: DisplayManager.prototype,
  };

  /**
   * Initializes the UserManager.
   */
  UserManager.initialize = function() {
    cr.ui.login.DisplayManager.initialize();
    login.AccountPickerScreen.register();
    cr.ui.Bubble.decorate($('bubble'));

    signin.ProfileBrowserProxyImpl.getInstance().initializeUserManager(
        window.location.hash);
  };

  /**
   * Shows the given screen.
   * @param {boolean} showGuest True if 'Browse as Guest' button should be
   *     displayed.
   * @param {boolean} showAddPerson True if 'Add Person' button should be
   *     displayed.
   */
  UserManager.showUserManagerScreen = function(showGuest, showAddPerson) {
    UserManager.getInstance().showScreen({id: 'account-picker',
                                          data: {disableAddUser: false}});
    // Hide control options if the user does not have the right permissions.
    var controlBar = document.querySelector('control-bar');
    controlBar.showGuest = showGuest;
    controlBar.showAddPerson = showAddPerson;

    // Disable the context menu, as the Print/Inspect element items don't
    // make sense when displayed as a widget.
    document.addEventListener('contextmenu', function(e) {e.preventDefault();});

    // TODO(mahmadi): start the tutorial if the location hash is #tutorial.
  };

  /**
   * Open a new browser for the given profile.
   * @param {string} profilePath The profile's path.
   */
  UserManager.launchUser = function(profilePath) {
    signin.ProfileBrowserProxyImpl.getInstance().launchUser(profilePath);
  };

  /**
   * Disables signin UI.
   */
  UserManager.disableSigninUI = function() {
    DisplayManager.disableSigninUI();
  };

  /**
   * Shows signin UI.
   * @param {string=} opt_email An optional email for signin UI.
   */
  UserManager.showSigninUI = function(opt_email) {
    DisplayManager.showSigninUI(opt_email);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attempts tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  UserManager.showSignInError = function(loginAttempts, message, link, helpId) {
    DisplayManager.showSignInError(loginAttempts, message, link, helpId);
  };

  /**
   * Clears error bubble as well as optional menus that could be open.
   */
  UserManager.clearErrors = function() {
    DisplayManager.clearErrors();
  };

  // Export
  return {
    UserManager: UserManager
  };
});

// Alias to Oobe for use in src/ui/login/account_picker/user_pod_row.js
var Oobe = cr.ui.UserManager;

// Allow selection events on components with editable text (password field)
// bug (http://code.google.com/p/chromium/issues/detail?id=125863)
disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
         src instanceof HTMLInputElement &&
         /text|password|search/.test(src.type);
});

document.addEventListener('DOMContentLoaded', cr.ui.UserManager.initialize);
