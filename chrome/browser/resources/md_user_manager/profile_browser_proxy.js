// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper object and related behavior that encapsulate messaging
 * between JS and C++ for creating/importing profiles in the user-manager page.
 */

/** @typedef {{username: string, profilePath: string}} */
var SignedInUser;

/** @typedef {{name: string, filePath: string, isSupervised: boolean}} */
var ProfileInfo;

/** @typedef {{id: string,
 *             name: string,
 *             iconURL: string,
 *             onCurrentDevice: boolean}}
 */
var SupervisedUser;

cr.define('signin', function() {
  /** @interface */
  function ProfileBrowserProxy() {}

  ProfileBrowserProxy.prototype = {
    /**
     * Gets the available profile icons to choose from.
     */
    getAvailableIcons: function() {
      assertNotReached();
    },

    /**
     * Gets the current signed-in users.
     */
    getSignedInUsers: function() {
      assertNotReached();
    },

    /**
     * Launches the guest user.
     */
    launchGuestUser: function() {
      assertNotReached();
    },

    /**
     * Gets the list of existing supervised users.
     * @param {string} profilePath Profile Path of the custodian.
     * @return {Promise} A promise for the requested data.
     * @private
     */
    getExistingSupervisedUsers: function(profilePath) {
      assertNotReached();
    },

    /**
     * Creates a profile.
     * @param {string} profileName Name of the new profile.
     * @param {string} profileIconUrl URL of the selected icon of the new
     *     profile.
     * @param {boolean} isSupervised True if the new profile is supervised.
     * @param {string} supervisedUserId ID of the supervised user to be
     *     imported.
     * @param {string} custodianProfilePath Profile path of the custodian if
     *     the new profile is supervised.
     */
    createProfile: function(profileName, profileIconUrl, isSupervised,
        supervisedUserId, custodianProfilePath) {
      assertNotReached();
    },

    /**
     * Cancels creation of the new profile.
     */
    cancelCreateProfile: function() {
      assertNotReached();
    },

    /**
     * Initializes the UserManager
     * @param {string} locationHash
     */
    initializeUserManager: function(locationHash) {
      assertNotReached();
    },

    /**
     * Launches the user with the given |profilePath|
     * @param {string} profilePath Profile Path of the user.
     */
    launchUser: function(profilePath) {
      assertNotReached();
    },

    /**
     * Opens the given url in a new tab in the browser instance of the last
     * active profile. Hyperlinks don't work in the user manager since its
     * browser instance does not support tabs.
     * @param {string} url
     */
    openUrlInLastActiveProfileBrowser: function(url) {
      assertNotReached();
    },
  };

  /**
   * @constructor
   * @implements {signin.ProfileBrowserProxy}
   */
  function ProfileBrowserProxyImpl() {}

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(ProfileBrowserProxyImpl);

  ProfileBrowserProxyImpl.prototype = {
    /** @override */
    getAvailableIcons: function() {
      chrome.send('requestDefaultProfileIcons');
    },

    /** @override */
    getSignedInUsers: function() {
      chrome.send('requestSignedInProfiles');
    },

    /** @override */
    launchGuestUser: function() {
      chrome.send('launchGuest');
    },

    /** @override */
    getExistingSupervisedUsers: function(profilePath) {
      return cr.sendWithPromise('getExistingSupervisedUsers', profilePath);
    },

    /** @override */
    createProfile: function(profileName, profileIconUrl, isSupervised,
        supervisedUserId, custodianProfilePath) {
      chrome.send('createProfile',
                  [profileName, profileIconUrl, false, isSupervised,
                   supervisedUserId, custodianProfilePath]);
    },

    /** @override */
    cancelCreateProfile: function() {
      chrome.send('cancelCreateProfile');
    },

    /** @override */
    initializeUserManager: function(locationHash) {
      chrome.send('userManagerInitialize', [locationHash]);
    },

    /** @override */
    launchUser: function(profilePath) {
      chrome.send('launchUser', [profilePath]);
    },

    /** @override */
    openUrlInLastActiveProfileBrowser: function(url) {
      chrome.send('openUrlInLastActiveProfileBrowser', [url]);
    },
  };

  return {
    ProfileBrowserProxy: ProfileBrowserProxy,
    ProfileBrowserProxyImpl: ProfileBrowserProxyImpl,
  };
});
