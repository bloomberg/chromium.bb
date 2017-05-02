// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for settings_ui.
 */

/**
 * Specifies page visibility. Only pages that have conditional visibility
 * (e.g. in Guest mode) have defined entries.
 * @typedef {{
 *   advancedSettings: (boolean|undefined),
 *   androidApps: (boolean|undefined),
 *   appearance: (boolean|undefined|AppearancePageVisibility),
 *   dateTime: (boolean|undefined|DateTimePageVisibility),
 *   defaultBrowser: (boolean|undefined),
 *   downloads: (undefined|DownloadsPageVisibility),
 *   onStartup: (boolean|undefined),
 *   passwordsAndForms: (boolean|undefined),
 *   people: (boolean|undefined),
 *   privacy: (undefined|PrivacyPageVisibility),
 *   reset:(boolean|undefined),
 * }}
 */
var PageVisibility;

/**
 * @typedef {{
 *   bookmarksBar: (boolean|undefined),
 *   homeButton: (boolean|undefined),
 *   pageZoom: (boolean|undefined),
 *   setTheme: (boolean|undefined),
 *   setWallpaper: (boolean|undefined),
 * }}
 */
var AppearancePageVisibility;

/**
 * @typedef {{
 *   timeZoneSelector: (boolean|undefined),
 * }}
 */
var DateTimePageVisibility;

/**
 * @typedef {{
 *   googleDrive: (boolean|undefined)
 * }}
 */
var DownloadsPageVisibility;

/**
 * @typedef {{
 *   networkPrediction: (boolean|undefined),
 *   searchPrediction: (boolean|undefined),
 * }}
 */
var PrivacyPageVisibility;

// TODO(mahmadi): Dummy code for closure compiler to process this file.
(function foo() {})();
