// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used for testing the Device page. */
cr.exportPath('settings');

/**
 * Mirrors DeviceType from ash/system/power/power_status.h.
 * @enum {number}
 */
settings.PowerDeviceType = {
  DEDICATED_CHARGER: 0,
  DUAL_ROLE_USB: 1,
};

/**
 * @typedef {{
 *   id: string,
 *   type: settings.PowerDeviceType,
 *   description: string
 * }}
 */
settings.PowerSource;

/**
 * @typedef {{
 *   charging: boolean,
 *   calculating: boolean,
 *   percent: number,
 *   statusText: string,
 * }}
 */
settings.BatteryStatus;

/**
 * Mirrors chromeos::settings::PowerHandler::IdleBehavior.
 * @enum {number}
 */
settings.IdleBehavior = {
  DISPLAY_OFF_SLEEP: 0,
  DISPLAY_OFF_STAY_AWAKE: 1,
  DISPLAY_ON: 2,
  OTHER: 3,
};

/**
 * Mirrors chromeos::PowerPolicyController::Action.
 * @enum {number}
 */
settings.LidClosedBehavior = {
  SUSPEND: 0,
  STOP_SESSION: 1,
  SHUT_DOWN: 2,
  DO_NOTHING: 3,
};

/**
 * @typedef {{
 *   idleBehavior: settings.IdleBehavior,
 *   idleControlled: boolean,
 *   lidClosedBehavior: settings.LidClosedBehavior,
 *   lidClosedControlled: boolean,
 *   hasLid: boolean,
 * }}
 */
settings.PowerManagementSettings;

/**
 * @typedef {{name:string,
 *            value:string,
 *            preferred:boolean,
 *            supportsLockScreen: boolean}}
 */
settings.NoteAppInfo;

cr.define('settings', function() {
  /** @interface */
  function DevicePageBrowserProxy() {}

  DevicePageBrowserProxy.prototype = {
    /** Initializes the mouse and touchpad handler. */
    initializePointers: function() {},

    /** Initializes the stylus handler. */
    initializeStylus: function() {},

    /**
     * Override to interact with the on-tap/on-keydown event on the Learn More
     * link.
     * @param {!Event} e
     */
    handleLinkEvent: function(e) {},

    /** Initializes the keyboard WebUI handler. */
    initializeKeyboard: function() {},

    /** Shows the Ash keyboard shortcuts overlay. */
    showKeyboardShortcutsOverlay: function() {},

    /** Requests a power status update. */
    updatePowerStatus: function() {},

    /**
     * Sets the ID of the power source to use.
     * @param {string} powerSourceId ID of the power source. '' denotes the
     *     battery (no external power source).
     */
    setPowerSource: function(powerSourceId) {},

    /** Requests the current power management settings. */
    requestPowerManagementSettings: function() {},

    /**
     * Sets the idle power management behavior.
     * @param {settings.IdleBehavior} behavior Idle behavior.
     */
    setIdleBehavior: function(behavior) {},

    /**
     * Sets the lid-closed power management behavior.
     * @param {settings.LidClosedBehavior} behavior Lid-closed behavior.
     */
    setLidClosedBehavior: function(behavior) {},

    /**
     * |callback| is run when there is new note-taking app information
     * available or after |requestNoteTakingApps| has been called.
     * @param {function(Array<settings.NoteAppInfo>, boolean):void} callback
     */
    setNoteTakingAppsUpdatedCallback: function(callback) {},

    /**
     * Open up the play store with the given URL.
     * @param {string} url
     */
    showPlayStore: function(url) {},

    /**
     * Request current note-taking app info. Invokes any callback registered in
     * |onNoteTakingAppsUpdated|.
     */
    requestNoteTakingApps: function() {},

    /**
     * Changes the preferred note taking app.
     * @param {string} appId The app id. This should be a value retrieved from a
     *     |onNoteTakingAppsUpdated| callback.
     */
    setPreferredNoteTakingApp: function(appId) {},
  };

  /**
   * @constructor
   * @implements {settings.DevicePageBrowserProxy}
   */
  function DevicePageBrowserProxyImpl() {}
  cr.addSingletonGetter(DevicePageBrowserProxyImpl);

  DevicePageBrowserProxyImpl.prototype = {
    /** @override */
    initializePointers: function() {
      chrome.send('initializePointerSettings');
    },

    /** @override */
    initializeStylus: function() {
      chrome.send('initializeStylusSettings');
    },

    /** override */
    handleLinkEvent: function(e) {
      // Prevent the link from activating its parent element when tapped or
      // when Enter is pressed.
      if (e.type != 'keydown' || e.keyCode == 13)
        e.stopPropagation();
    },

    /** @override */
    initializeKeyboard: function() {
      chrome.send('initializeKeyboardSettings');
    },

    /** @override */
    showKeyboardShortcutsOverlay: function() {
      chrome.send('showKeyboardShortcutsOverlay');
    },

    /** @override */
    updatePowerStatus: function() {
      chrome.send('updatePowerStatus');
    },

    /** @override */
    setPowerSource: function(powerSourceId) {
      chrome.send('setPowerSource', [powerSourceId]);
    },

    /** @override */
    requestPowerManagementSettings: function() {
      chrome.send('requestPowerManagementSettings');
    },

    /** @override */
    setIdleBehavior: function(behavior) {
      chrome.send('setIdleBehavior', [behavior]);
    },

    /** @override */
    setLidClosedBehavior: function(behavior) {
      chrome.send('setLidClosedBehavior', [behavior]);
    },

    /** @override */
    setNoteTakingAppsUpdatedCallback: function(callback) {
      cr.addWebUIListener('onNoteTakingAppsUpdated', callback);
    },

    /** @override */
    showPlayStore: function(url) {
      chrome.send('showPlayStoreApps', [url]);
    },

    /** @override */
    requestNoteTakingApps: function() {
      chrome.send('requestNoteTakingApps');
    },

    /** @override */
    setPreferredNoteTakingApp: function(appId) {
      chrome.send('setPreferredNoteTakingApp', [appId]);
    },
  };

  return {
    DevicePageBrowserProxy: DevicePageBrowserProxy,
    DevicePageBrowserProxyImpl: DevicePageBrowserProxyImpl,
  };
});
