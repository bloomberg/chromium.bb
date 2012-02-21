// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Font settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.FontSettings

var fs = chrome.experimental.fontSettings;

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function getPerScriptFontName() {
    var expected = 'UnBatang';
    var message = 'Setting for Hangul standard font should be ' + expected;
    fs.getFontName({
      script: 'Hang',
      genericFamily: 'standard'
    }, expect({fontName: expected}, message));
  },

  function getGlobalFontName() {
    var expected = 'Arial';
    var message =
        'Setting for global sansserif font should be ' + expected;
    fs.getFontName({
      genericFamily: 'sansserif'
    }, expect({fontName: expected}, message));
  }
]);
