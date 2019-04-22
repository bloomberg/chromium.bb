// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Specifies page visibility based on incognito status, Chrome OS guest mode,
 * and whether or not to include OS settings. Once the Chrome OS SplitSettings
 * project is completed this can be changed to only consider incognito and
 * guest mode. https://crbug.com/950007
 * @typedef {{
 *   advancedSettings: (boolean|undefined),
 *   appearance: (boolean|undefined|AppearancePageVisibility),
 *   autofill: (boolean|undefined),
 *   bluetooth: (boolean|undefined),
 *   dateTime: (boolean|undefined|DateTimePageVisibility),
 *   defaultBrowser: (boolean|undefined),
 *   device: (boolean|undefined),
 *   downloads: (boolean|undefined|DownloadsPageVisibility),
 *   internet: (boolean|undefined),
 *   multidevice: (boolean|undefined),
 *   onStartup: (boolean|undefined),
 *   people: (boolean|undefined),
 *   privacy: (boolean|undefined|PrivacyPageVisibility),
 *   reset:(boolean|undefined),
 * }}
 */
let PageVisibility;

/**
 * @typedef {{
 *   bookmarksBar: boolean,
 *   homeButton: boolean,
 *   pageZoom: boolean,
 *   setTheme: boolean,
 *   setWallpaper: boolean,
 * }}
 */
let AppearancePageVisibility;

/**
 * @typedef {{
 *   timeZoneSelector: boolean,
 * }}
 */
let DateTimePageVisibility;

/**
 * @typedef {{
 *   googleDrive: boolean
 * }}
 */
let DownloadsPageVisibility;

/**
 * @typedef {{
 *   networkPrediction: boolean,
 *   searchPrediction: boolean,
 * }}
 */
let PrivacyPageVisibility;

cr.define('settings', function() {
  /**
   * Dictionary defining page visibility.
   * @type {!PageVisibility}
   */
  let pageVisibility;

  const isGuest = loadTimeData.getBoolean('isGuest');

  // "if not chromeos" and "if chromeos" in two completely separate blocks
  // to work around closure compiler.
  // <if expr="not chromeos">
  pageVisibility = {
    autofill: !isGuest,
    people: !isGuest,
    onStartup: !isGuest,
    reset: !isGuest,
    appearance: !isGuest,
    defaultBrowser: !isGuest,
    advancedSettings: !isGuest,
    extensions: !isGuest,
  };
  // </if>
  // <if expr="chromeos">
  const showOS = loadTimeData.getBoolean('showOSSettings');
  const showBrowser = loadTimeData.getBoolean('showBrowserSettings');
  pageVisibility = {
    internet: showOS,
    bluetooth: showOS,
    multidevice: showOS && !isGuest,
    autofill: showBrowser && !isGuest,
    people: !isGuest,
    onStartup: !isGuest,
    reset: !isGuest,
    appearance: {
      setWallpaper: showOS && !isGuest,
      setTheme: showBrowser && !isGuest,
      homeButton: showBrowser && !isGuest,
      bookmarksBar: showBrowser && !isGuest,
      pageZoom: showBrowser && !isGuest,
    },
    device: showOS,
    advancedSettings: true,
    privacy: {
      searchPrediction: !isGuest,
      networkPrediction: !isGuest,
    },
    downloads: {
      googleDrive: !isGuest,
    },
    extensions: showBrowser && !isGuest,
  };
  // </if>

  return {pageVisibility: pageVisibility};
});
