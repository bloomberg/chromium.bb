// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Chrome OS Contained Home API definition.
 */

/**
 * Namespace for the contained home bridge and related data.
 * @const
 */
var containedHome = {};

/**
 * System bridge API for the contained home experience.
 *
 * @interface
 */
containedHome.Bridge = class {
  /**
   * Adds listener for system events.
   * @param {!containedHome.Listener} listener Listener for system events.
   */
  addListener(listener) {}

  /**
   * Returns an access token with scope for the contained home experience.
   * @return {!Promise<string>} Promise for the access token.
   */
  getAccessToken() {}

  /**
   * Returns a list of apps installed in the user session.
   * @return {!Promise<!Array<!containedHome.InstalledApp>>} Promise for the
   *     list of apps.
   */
  getInstalledApps() {}

  /**
   * Launches a content (app, video, etc).
   * @param {!containedHome.ContentSource} contentSource
   * @param {string} contentId
   * @param {?Object=} opt_params Optional params to locate the content.
   * @return {!Promise<boolean>} Promise that is resolved after the content is
   *     launched.
   */
  launchContent(contentSource, contentId, opt_params) {}
};

/**
 * Set of known / handled content sources.
 *
 * A "Content Source" describes how to launch/view the content.
 * @enum {string}
 */
containedHome.ContentSource = {
  /** The content is, or is hosted inside, an ARC++ app. */
  ARC_INTENT: 'arc_intent',
};

/**
 * Types of installed apps on ChromeOS.
 * @enum {string}
 */
containedHome.AppType = {
  /** The app is an ARC++ app (Android app). */
  ARC: 'arc',
};

/**
 * A record representing an installed app on the system.
 * @record
 */
containedHome.InstalledApp = class {
  constructor() {
    /** @type {!containedHome.AppType} The type of app. */
    this.appType;
    /**
     * @type {string} Stable, unique identifier for the app. For ARC++ apps,
     *     this is the package name.
     */
    this.appId;
    /** @type {string} Readable name to display. */
    this.displayName;
    /** @type {string | undefined} Base64-encoded thumbnail image, fallback. */
    this.thumbnailImage;
    /** @type {boolean | undefined} Whether the app is suspended. */
    this.suspended;
  }
};

/**
 * Different ways an installed app can change.
 * @enum {string}
 */
containedHome.AppEventType = {
  INSTALLED: 'installed',
  UNINSTALLED: 'uninstalled',
};

/**
 * Interface for a listener of system events, subscribed via
 * {!containedHome.Bridge}.
 *
 * @interface
 */
containedHome.Listener = class {
  /**
   * Called when an app state change.
   * @param {!containedHome.InstalledApp} app The app whose state changed.
   * @param {!containedHome.AppEventType} appEventType Type of the event
   *     indicating what changed for the app.
   */
  onInstalledAppChanged(app, appEventType) {}
};

/**
 * Provides bridge implementation.
 * @return {!containedHome.Bridge} Bridge instance that can be used to interact
 *     with ChromeOS.
 */
containedHome.getChromeOsBridge = function() {};
