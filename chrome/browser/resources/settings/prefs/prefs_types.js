// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Types for CrSettingsPrefsElement.
 */

if (CrSettingsPrefs === undefined) {
  var CrSettingsPrefs = {};

  /**
   * The type of the event fired when prefs have been fetched and initialized.
   * @const {string}
   */
  CrSettingsPrefs.INITIALIZED = 'cr-settings-prefs-initialized';

  /**
   * Global boolean set to true when all settings have been initialized.
   * @type {boolean}
   */
  CrSettingsPrefs.isInitialized = false;
}
