// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Typedefs for CrSettingsPrefsElement.
 */

var CrSettingsPrefs = {};

/**
 * @typedef {{
 *   a11y: CrSettingsPrefs.A11y,
 *   touchpad: CrSettingsPrefs.Touchpad,
 *   accessibility: boolean
 * }}
 */
CrSettingsPrefs.Settings;

/**
 * @typedef {{
 *   enableMenu: boolean,
 *   largeCursorEnabled: boolean,
 *   highContrastEnabled: boolean,
 *   stickyKeysEnabled: boolean,
 *   screenMagnifier: boolean,
 *   autoclick: boolean,
 *   autoclickDelayMs: number,
 *   virtualKeyboard: boolean
 * }}
 */
CrSettingsPrefs.A11y;

/**
 * @typedef {{
 *   enableTapDragging: boolean
 * }}
 */
CrSettingsPrefs.Touchpad;
