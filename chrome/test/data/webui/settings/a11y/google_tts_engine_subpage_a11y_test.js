// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the
 * MANAGE_GOOGLE_TTS_ENGINE_SETTINGS route.
 */

// This is only for Chrome OS.
GEN('#if defined(OS_CHROMEOS)');

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

GoogleTtsEngineAccessibilityTest = class extends SettingsAccessibilityTest {
  /** @override */
  get commandLineSwitches() {
    return ['enable-experimental-a11y-features'];
  }

  /** @override */
  get browsePreload() {
    return 'chrome://settings/manageAccessibility/tts/googleTtsEngine';
  }
};

AccessibilityTest.define('GoogleTtsEngineAccessibilityTest', {
  /** @override */
  name: 'MANAGE_GOOGLE_TTS_ENGINE_SETTINGS',
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptions,
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: SettingsAccessibilityTest.violationFilter,
});

GEN('#endif  // defined(OS_CHROMEOS)');
