// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Chrome OS Kiosk Next Home API definition.
 */

/**
 * Namespace for the Kiosk Next Home bridge and related data.
 * @const
 */
var kioskNextHome = {};

/**
 * System bridge API for the Kiosk Next Home.
 *
 * @interface
 */
kioskNextHome.Bridge = class {
  /**
   * Adds listener for system events.
   * @param {!kioskNextHome.Listener} listener Listener for system events.
   */
  addListener(listener) {}

  /**
   * Gets the obfuscated account Gaia ID associated with the current user
   * session.
   * @return {!Promise<string>} Promise for the obfuscated account Gaia ID.
   */
  getAccountId() {}

  /**
   * Returns an access token with the requested scopes.
   * @param {!Array<string>} scopes List of scopes to use when obtaining access
   *     token.
   * @return {!Promise<string>} Promise for the access token.
   */
  getAccessToken(scopes) {}

  /**
   * Returns the Android ID for the ARC++ container.
   * @return {!Promise<string>} Promise for the Android ID.
   */
  getAndroidId() {}

  /**
   * Returns a list of apps installed in the user session.
   * @return {!Promise<!Array<!kioskNextHome.App>>} Promise for the list of
   *     apps.
   */
  getApps() {}

  /**
   * Launches an app.
   * @param {string} appId Chrome OS identifier for the app.
   * @return {!Promise} Resolves when app is launched, or rejects in case of
   *     failures.
   */
  launchApp(appId) {}

  /**
   * Launches an ARC++ intent.
   * @param {string} intent The URI of an ARC++ intent.
   * @return {!Promise} Resolves when intent is launched, or rejects in case of
   *     failures.
   */
  launchIntent(intent) {}

  /**
   * Uninstalls an app.
   * @param {string} appId App to uninstall.
   * @return {!Promise} Resolves when app is uninstalled, or rejects in case of
   *     failures.
   */
  uninstallApp(appId) {}

  /**
   * Returns current device network state.
   * @return {kioskNextHome.NetworkState}
   */
  getNetworkState() {}
};

/**
 * Types of installed apps on Chrome OS.
 * @enum {string}
 */
kioskNextHome.AppType = {
  /** The app is an ARC++ app (Android app). */
  ARC: 'arc',
  /** The content is a Chrome app. */
  CHROME: 'chrome',
};

/**
 * Readiness status for apps.
 * These values loosely map to AppService's apps.mojom.Readiness enum.
 * @enum {string}
 */
kioskNextHome.AppReadiness = {
  /** Installed and launchable. */
  READY: 'ready',
  /** App is disabled by policy. */
  DISABLED: 'disabled',
  /** App was uninstalled by user. */
  UNINSTALLED: 'uninstalled',
};

/**
 * A record representing an installed app on the system.
 * @record
 */
kioskNextHome.App = class {
  constructor() {
    /** @type {string} Unique Chrome OS identifier for the app. */
    this.appId;
    /** @type {kioskNextHome.AppType} */
    this.type;
    /**
     * @type {string | undefined} Android package name, if it's an ARC++ app.
     */
    this.packageName;
    /** @type {string} Readable name to display. */
    this.displayName;
    /** @type {string | undefined} Base64-encoded thumbnail image, fallback. */
    this.thumbnailImage;
    /**
     * @type {kioskNextHome.AppReadiness} Current readiness state for the app.
     */
    this.readiness;
  }
};

/**
 * Current network state of the device.
 * @enum {string}
 */
kioskNextHome.NetworkState = {
  ONLINE: 'online',
  OFFLINE: 'offline',
};

/**
 * Interface for a listener of system events, subscribed via
 * {!kioskNextHome.Bridge}.
 *
 * @interface
 */
kioskNextHome.Listener = class {
  /**
   * Called when an app state changes.
   * TODO(brunoad): Adapt for AppService calls.
   * @param {!kioskNextHome.App} app The app whose state changed.
   */
  onAppChanged(app) {}

  /**
   * Called when the network state changes.
   * @param {kioskNextHome.NetworkState} networkState Current network state of
   *     the device.
   */
  onNetworkStateChanged(networkState) {}
};

/**
 * Provides bridge implementation.
 * @return {!kioskNextHome.Bridge} Bridge instance that can be used to interact
 *     with Chrome OS.
 */
kioskNextHome.getChromeOsBridge = function() {};
