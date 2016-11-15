// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The mock signin.ProfileBrowserProxy.
 *
 * @constructor
 * @implements {signin.ProfileBrowserProxy}
 * @extends {settings.TestBrowserProxy}
 */
var TestProfileBrowserProxy = function() {
  settings.TestBrowserProxy.call(this, [
    'getAvailableIcons',
    'getSignedInUsers',
    'launchGuestUser',
    'createProfile',
    'cancelCreateProfile',
    'initializeUserManager',
    'launchUser',
    'getExistingSupervisedUsers',
    'areAllProfilesLocked',
  ]);

  /** @private {!Array<!AvatarIcon>} */
  this.icons_ = [];

  /** @private {!Array<SignedInUser>} */
  this.signedInUsers_ = [];

  /** @private {!ProfileInfo} */
  this.defaultProfileInfo_ = {};

  /** @private {!Array<SupervisedUser>} */
  this.existingSupervisedUsers_ = [];

  /** @private {boolean} */
  this.allProfilesLocked_ = false;
};

TestProfileBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /**
   * @param {!Array<!AvatarIcon>} icons
   */
  setIcons: function(icons) {
    this.icons_ = icons;
  },

  /**
   * @param {!Array<SignedInUser>} signedInUsers
   */
  setSignedInUsers: function(signedInUsers) {
    this.signedInUsers_ = signedInUsers;
  },

  /**
   * @param {!ProfileInfo} profileInfo
   */
  setDefaultProfileInfo: function(profileInfo) {
    this.defaultProfileInfo_ = profileInfo;
  },

  /**
   * @param {!Array<SupervisedUser>} supervisedUsers
   */
  setExistingSupervisedUsers: function(supervisedUsers) {
    this.existingSupervisedUsers_ = supervisedUsers;
  },

  /**
   * @param {boolean} allProfilesLocked
   */
  setAllProfilesLocked: function(allProfilesLocked) {
    this.allProfilesLocked_ = allProfilesLocked;
  },

  /** @override */
  getAvailableIcons: function() {
    this.methodCalled('getAvailableIcons');
    cr.webUIListenerCallback('profile-icons-received', this.icons_);
    cr.webUIListenerCallback('profile-defaults-received',
                             this.defaultProfileInfo_);
  },

  /** @override */
  getSignedInUsers: function() {
    this.methodCalled('getSignedInUsers');
    cr.webUIListenerCallback('signedin-users-received', this.signedInUsers_);
  },

  /** @override */
  cancelCreateProfile: function() {
    /**
     * Flag used to test whether this method was not called.
     * @type {boolean}
     */
    this.cancelCreateProfileCalled = true;
    this.methodCalled('cancelCreateProfile');
  },

  /** @override */
  createProfile: function(profileName, profileIconUrl, createShortcut,
        isSupervised, supervisedUserId, custodianProfilePath) {
    this.methodCalled('createProfile',
                      {profileName: profileName,
                       profileIconUrl: profileIconUrl,
                       createShortcut: createShortcut,
                       isSupervised: isSupervised,
                       supervisedUserId: supervisedUserId,
                       custodianProfilePath: custodianProfilePath});
  },

  /** @override */
  launchGuestUser: function() {
    this.methodCalled('launchGuestUser');
  },

  /** @override */
  getExistingSupervisedUsers: function() {
    this.methodCalled('getExistingSupervisedUsers');
    return Promise.resolve(this.existingSupervisedUsers_);
  },

  /** @override */
  areAllProfilesLocked: function() {
    this.methodCalled('areAllProfilesLocked');
    return Promise.resolve(this.allProfilesLocked_);
  },
};
