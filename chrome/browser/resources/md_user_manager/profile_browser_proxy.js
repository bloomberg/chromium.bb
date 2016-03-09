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
     * Creates a profile.
     * @param {string} profileName Name of the new profile.
     * @param {string} profileIconUrl URL of the selected icon of the new
     *     profile.
     * @param {boolean} isSupervised True if the new profile is supervised.
     * @param {string|undefined} supervisorProfilePath Profile path of the
     *     supervisor if the new profile is supervised.
     */
    createProfile: function(profileName, profileIconUrl, isSupervised,
        supervisorProfilePath) {
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
    }
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
    createProfile: function(profileName, profileIconUrl, isSupervised,
        supervisorProfilePath) {
      chrome.send('createProfile',
                  [profileName, profileIconUrl, false, isSupervised, '',
                   supervisorProfilePath]);
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
    }
  };

  return {
    ProfileBrowserProxy: ProfileBrowserProxy,
    ProfileBrowserProxyImpl: ProfileBrowserProxyImpl,
  };
});
