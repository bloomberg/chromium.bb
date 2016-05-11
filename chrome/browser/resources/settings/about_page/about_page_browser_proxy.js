// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "About" section to interact with
 * the browser.
 */

<if expr="chromeos">
/**
 * @typedef {{
 *   text: string,
 *   url: string,
 * }}
 */
var RegulatoryInfo;

/**
 * @typedef {{
 *   arcVersion: string,
 *   osFirmware: string,
 *   osVersion: string,
 * }}
 */
var VersionInfo;
</if>

cr.define('settings', function() {
  /**
   * Enumeration of all possible browser channels.
   * @enum {string}
   */
  var BrowserChannel = {
    BETA: 'beta-channel',
    DEV: 'dev-channel',
    STABLE: 'stable-channel',
  };

  /** @interface */
  function AboutPageBrowserProxy() {}

  AboutPageBrowserProxy.prototype = {
    /**
     * Called once the page is ready. It results in one or more
     * 'update-status-changed' WebUI events.
     */
    refreshUpdateStatus: function() {},

    /**
     * Relaunches the browser.
     */
    relaunchNow: function() {},

    /** Opens the help page. */
    openHelpPage: function() {},

<if expr="_google_chrome">
    /**
     * Opens the feedback dialog.
     */
    openFeedbackDialog: function() {},
</if>

<if expr="chromeos">
    /**
     * Checks for available update and applies if it exists.
     */
    requestUpdate: function() {},

    /**
     * @param {!BrowserChannel} channel
     * @param {boolean} isPowerwashAllowed
     */
    setChannel: function(channel, isPowerwashAllowed) {},

    /** @return {!Promise<string>} */
    getCurrentChannel: function() {},

    /** @return {!Promise<string>} */
    getTargetChannel: function() {},

    /** @return {!Promise<!VersionInfo>} */
    getVersionInfo: function() {},

    /** @return {!Promise<?RegulatoryInfo>} */
    getRegulatoryInfo: function() {},
</if>
  };

  /**
   * @implements {settings.AboutPageBrowserProxy}
   * @constructor
   */
  function AboutPageBrowserProxyImpl() {}
  cr.addSingletonGetter(AboutPageBrowserProxyImpl);

  AboutPageBrowserProxyImpl.prototype = {
    /** @override */
    refreshUpdateStatus: function() {
      chrome.send('refreshUpdateStatus');
    },

    /** @override */
    relaunchNow: function() {
      chrome.send('relaunchNow');
    },

    /** @override */
    openHelpPage: function() {
      chrome.send('openHelpPage');
    },

<if expr="_google_chrome">
    /** @override */
    openFeedbackDialog: function() {
      chrome.send('openFeedbackDialog');
    },
</if>

<if expr="chromeos">
    /** @override */
    requestUpdate: function() {
      chrome.send('requestUpdate');
    },

    /** @override */
    setChannel: function(channel, isPowerwashAllowed) {
      chrome.send('setChannel', [channel, isPowerwashAllowed]);
    },

    /** @override */
    getCurrentChannel: function() {
      return cr.sendWithPromise('getCurrentChannel');
    },

    /** @override */
    getTargetChannel: function() {
      return cr.sendWithPromise('getTargetChannel');
    },

    /** @override */
    getVersionInfo: function() {
      return cr.sendWithPromise('getVersionInfo');
    },

    /** @override */
    getRegulatoryInfo: function() {
      return cr.sendWithPromise('getRegulatoryInfo');
    }
</if>
  };

  return {
    AboutPageBrowserProxy: AboutPageBrowserProxy,
    AboutPageBrowserProxyImpl: AboutPageBrowserProxyImpl,
  };
});
