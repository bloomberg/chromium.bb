// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Types for CrSettingsPrefsElement.
 */

/** @type {{INITIALIZED: string, isInitialized: boolean}} */
var CrSettingsPrefs;

// TODO(michaelpg): provide a Promise for initialization.
if (CrSettingsPrefs === undefined) {
  CrSettingsPrefs = {
    /**
     * The name of the event fired when prefs have been fetched and initialized.
     * @const {string}
     */
    INITIALIZED: 'cr-settings-prefs-initialized',

    /**
     * Global boolean set to true when all settings have been initialized.
     * @type {boolean}
     */
    isInitialized: false,
  };
}
