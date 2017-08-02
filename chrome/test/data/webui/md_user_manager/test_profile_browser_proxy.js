// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The mock signin.ProfileBrowserProxy.
 * @implements {signin.ProfileBrowserProxy}
 */
class TestProfileBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
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
  }

  /**
   * @param {!Array<!AvatarIcon>} icons
   */
  setIcons(icons) {
    this.icons_ = icons;
  }

  /**
   * @param {!Array<SignedInUser>} signedInUsers
   */
  setSignedInUsers(signedInUsers) {
    this.signedInUsers_ = signedInUsers;
  }

  /**
   * @param {!ProfileInfo} profileInfo
   */
  setDefaultProfileInfo(profileInfo) {
    this.defaultProfileInfo_ = profileInfo;
  }

  /**
   * @param {!Array<SupervisedUser>} supervisedUsers
   */
  setExistingSupervisedUsers(supervisedUsers) {
    this.existingSupervisedUsers_ = supervisedUsers;
  }

  /**
   * @param {boolean} allProfilesLocked
   */
  setAllProfilesLocked(allProfilesLocked) {
    this.allProfilesLocked_ = allProfilesLocked;
  }

  /** @override */
  getAvailableIcons() {
    this.methodCalled('getAvailableIcons');
    cr.webUIListenerCallback('profile-icons-received', this.icons_);
    cr.webUIListenerCallback('profile-defaults-received',
                             this.defaultProfileInfo_);
  }

  /** @override */
  getSignedInUsers() {
    this.methodCalled('getSignedInUsers');
    cr.webUIListenerCallback('signedin-users-received', this.signedInUsers_);
  }

  /** @override */
  cancelCreateProfile() {
    /**
     * Flag used to test whether this method was not called.
     * @type {boolean}
     */
    this.cancelCreateProfileCalled = true;
    this.methodCalled('cancelCreateProfile');
  }

  /** @override */
  createProfile(
    profileName,
    profileIconUrl,
    createShortcut,
    isSupervised,
    supervisedUserId,
    custodianProfilePath
  ) {
    this.methodCalled('createProfile',
                      {profileName: profileName,
                       profileIconUrl: profileIconUrl,
                       createShortcut: createShortcut,
                       isSupervised: isSupervised,
                       supervisedUserId: supervisedUserId,
                       custodianProfilePath: custodianProfilePath});
  }

  /** @override */
  launchGuestUser() {
    this.methodCalled('launchGuestUser');
  }

  /** @override */
  getExistingSupervisedUsers() {
    this.methodCalled('getExistingSupervisedUsers');
    return Promise.resolve(this.existingSupervisedUsers_);
  }

  /** @override */
  areAllProfilesLocked() {
    this.methodCalled('areAllProfilesLocked');
    return Promise.resolve(this.allProfilesLocked_);
  }
}
