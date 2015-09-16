// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {Array<{key: string,
 *               type: chrome.settingsPrivate.PrefType,
 *               values: !Array<*>}>}
 * Test cases containing preference data. Each pref has three test values,
 * which can be used to change the pref. Intentionally, for a given pref, not
 * every test value is different from the one before it; this tests what
 * happens when stale changes are reported.
 */
var prefsTestCases = [{
  key: 'top_level_pref',
  type: chrome.settingsPrivate.PrefType.BOOLEAN,
  values: [true, false, true],
}, {
  key: 'browser.enable_flash',
  type: chrome.settingsPrivate.PrefType.BOOLEAN,
  values: [false, true, false],
}, {
  key: 'browser.enable_html5',
  type: chrome.settingsPrivate.PrefType.BOOLEAN,
  values: [true, false, false],
}, {
  key: 'device.overclock',
  type: chrome.settingsPrivate.PrefType.NUMBER,
  values: [0, .2, .6],
}, {
  key: 'browser.on.startup.homepage',
  type: chrome.settingsPrivate.PrefType.STRING,
  values: ['example.com', 'chromium.org', 'chrome.example.com'],
}, {
  key: 'profile.name',
  type: chrome.settingsPrivate.PrefType.STRING,
  values: ['Puppy', 'Puppy', 'Horsey'],
}];
