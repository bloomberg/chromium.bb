// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Manage Profile" subpage of
 * the People section to interact with the browser. Chrome Browser only.
 */

/**
 * Contains the possible profile shortcut statuses. These strings must be kept
 * in sync with the C++ Manage Profile handler.
 * @enum {string}
 */
var ProfileShortcutStatus = {
  PROFILE_SHORTCUT_SETTING_HIDDEN: 'profileShortcutSettingHidden',
  PROFILE_SHORTCUT_NOT_FOUND: 'profileShortcutNotFound',
  PROFILE_SHORTCUT_FOUND: 'profileShortcutFound',
};

cr.define('settings', function() {
  /** @interface */
  function ManageProfileBrowserProxy() {}

  ManageProfileBrowserProxy.prototype = {
    /**
     * Gets the available profile icons to choose from.
     * @return {!Promise<!Array<!AvatarIcon>>}
     */
    getAvailableIcons: function() {},

    /**
     * Sets the profile's icon to the GAIA avatar.
     */
    setProfileIconToGaiaAvatar: function() {},

    /**
     * Sets the profile's icon to one of the default avatars.
     * @param {string} iconUrl The new profile URL.
     */
    setProfileIconToDefaultAvatar: function(iconUrl) {},

    /**
     * Sets the profile's name.
     * @param {string} name The new profile name.
     */
    setProfileName: function(name) {},

    /**
     * Returns whether the current profile has a shortcut.
     * @return {!Promise<ProfileShortcutStatus>}
     */
    getProfileShortcutStatus: function() {},

    /**
     * Adds a shortcut for the current profile.
     */
    addProfileShortcut: function() {},

    /**
     * Removes the shortcut of the current profile.
     */
    removeProfileShortcut: function() {},
  };

  /**
   * @constructor
   * @implements {settings.ManageProfileBrowserProxy}
   */
  function ManageProfileBrowserProxyImpl() {}
  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(ManageProfileBrowserProxyImpl);

  ManageProfileBrowserProxyImpl.prototype = {
    /** @override */
    getAvailableIcons: function() {
      return cr.sendWithPromise('getAvailableIcons');
    },

    /** @override */
    setProfileIconToGaiaAvatar: function() {
      chrome.send('setProfileIconToGaiaAvatar');
    },

    /** @override */
    setProfileIconToDefaultAvatar: function(iconUrl) {
      chrome.send('setProfileIconToDefaultAvatar', [iconUrl]);
    },

    /** @override */
    setProfileName: function(name) {
      chrome.send('setProfileName', [name]);
    },

    /** @override */
    getProfileShortcutStatus: function() {
      return cr.sendWithPromise('requestProfileShortcutStatus');
    },

    /** @override */
    addProfileShortcut: function() {
      chrome.send('addProfileShortcut');
    },

    /** @override */
    removeProfileShortcut: function() {
      chrome.send('removeProfileShortcut');
    },
  };

  return {
    ManageProfileBrowserProxy: ManageProfileBrowserProxy,
    ManageProfileBrowserProxyImpl: ManageProfileBrowserProxyImpl,
  };
});
