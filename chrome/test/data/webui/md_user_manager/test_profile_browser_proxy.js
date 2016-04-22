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
  ]);

  /** @private {!Array<string>} */
  this.iconUrls_ = [];

  /** @private {!Array<SignedInUser>} */
  this.signedInUsers_ = [];

  /** @private {!ProfileInfo} */
  this.defaultProfileInfo_ = {};

  /** @private {!Array<SupervisedUser>} */
  this.existingSupervisedUsers_ = [];
};

TestProfileBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /**
   * @param {!Array<string>} iconUrls
   */
  setIconUrls: function(iconUrls) {
    this.iconUrls_ = iconUrls;
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

  /** @override */
  getAvailableIcons: function() {
    this.methodCalled('getAvailableIcons');
    cr.webUIListenerCallback('profile-icons-received', this.iconUrls_);
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
  createProfile: function(profileName, profileIconUrl, isSupervised,
        supervisedUserId, custodianProfilePath) {
    this.methodCalled('createProfile',
                      {profileName: profileName,
                       profileIconUrl: profileIconUrl,
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
};
