// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used for testing the Device page. */
cr.exportPath('settings');

/**
 * Mirrors DeviceType from ash/common/system/chromeos/power/power_status.h.
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

cr.define('settings', function() {
  /** @interface */
  function DevicePageBrowserProxy() {}

  DevicePageBrowserProxy.prototype = {
    /** Initializes the mouse and touchpad handler. */
    initializePointers: function() {},

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
  };

  return {
    DevicePageBrowserProxy: DevicePageBrowserProxy,
    DevicePageBrowserProxyImpl: DevicePageBrowserProxyImpl,
  };
});
