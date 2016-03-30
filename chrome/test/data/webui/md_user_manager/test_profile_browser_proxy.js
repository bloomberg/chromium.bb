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
    'launchUser'
  ]);

  /** @private {!Array<string>} */
  this.iconUrls_ = [];

  /** @private {!Array<SignedInUser>} */
  this.signedInUsers_ = [];
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

  /** @override */
  getAvailableIcons: function() {
    this.methodCalled('getAvailableIcons');
    cr.webUIListenerCallback('profile-icons-received', this.iconUrls_);
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
        supervisorProfilePath) {
    this.methodCalled('createProfile',
                      {profileName: profileName,
                       profileIconUrl: profileIconUrl,
                       isSupervised: isSupervised,
                       supervisorProfilePath: supervisorProfilePath});
  },

  /** @override */
  launchGuestUser: function() {
    this.methodCalled('launchGuestUser');
  }
};
