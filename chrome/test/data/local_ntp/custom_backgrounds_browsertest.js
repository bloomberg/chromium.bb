// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Tests local NTP custom backgrounds.
 */


/**
 * Local NTP's object for test and setup functions.
 */
test.customBackgrounds = {};


/**
 * Sets up the page for each individual test.
 */
test.customBackgrounds.setUp = function() {
  setUpPage('local-ntp-template');
};


// ******************************* SIMPLE TESTS *******************************
// These are run by runSimpleTests above.
// Functions from test_utils.js are automatically imported.


/**
 * Tests that the edit custom background button is visible if both
 * the flag is enabled and no custom theme is being used.
 */
test.customBackgrounds.testShowEditCustomBackground = function() {
  // Turn off voice search to avoid reinitializing the speech object
  configData.isVoiceSearchEnabled = false;

  configData.isCustomBackgroundsEnabled = true;
  getThemeBackgroundInfo = () => {return {usingDefaultTheme: true};};
  initLocalNTP(/*isGooglePage=*/true);
  assertTrue(elementIsVisible($('edit-bg')));
}


/**
 * Tests that the edit custom background button is not visible if
 * the flag is disabled.
 */
test.customBackgrounds.testHideEditCustomBackgroundFlag = function() {
  // Turn off voice search to avoid reinitializing the speech object
  configData.isVoiceSearchEnabled = false;

  configData.isCustomBackgroundsEnabled = false;
  initLocalNTP(/*isGooglePage=*/true);
  assertFalse(elementIsVisible($('edit-bg')));
}


/**
 * Tests that the edit custom background button is not visible if
 * a custom theme is being used.
 */
test.customBackgrounds.testHideEditCustomBackgroundTheme = function() {
  // Turn off voice search to avoid reinitializing the speech object
  configData.isVoiceSearchEnabled = false;

  getThemeBackgroundInfo = () => {return {usingDefaultTheme: false};};
  initLocalNTP(/*isGooglePage=*/true);
  assertFalse(elementIsVisible($('edit-bg')));
}
