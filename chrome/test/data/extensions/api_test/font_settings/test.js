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
  function setPerScriptFontName() {
    fs.setFontName({
      script: 'Hang',
      genericFamily: 'standard',
      fontName: 'Verdana'
    }, chrome.test.callbackPass());
  },

  function getPerScriptFontName() {
    var expected = 'Verdana';
    var message = 'Setting for Hangul standard font should be ' + expected;

    // This test may fail on Windows if the font is not installed on the
    // system. See crbug.com/122303
    fs.getFontName({
      script: 'Hang',
      genericFamily: 'standard'
    }, expect({fontName: expected}, message));
  },

  function setGlobalFontName() {
    fs.setFontName({
      genericFamily: 'sansserif',
      fontName: 'Tahoma'
    }, chrome.test.callbackPass());
  },

  function getGlobalFontName() {
    var expected = 'Tahoma';
    var message =
        'Setting for global sansserif font should be ' + expected;

    // As above, this test may fail on Windows if the font is not installed on
    // the system. See crbug.com/122303
    fs.getFontName({
      genericFamily: 'sansserif'
    }, expect({fontName: expected}, message));
  },

  function getFontList() {
    var message = 'getFontList should return an array of objects with ' +
        'fontName and localizedName properties.';
    fs.getFontList(chrome.test.callbackPass(function(value) {
      chrome.test.assertTrue(value.length > 0,
                             "Font list is not expected to be empty.");
      chrome.test.assertEq('string', typeof(value[0].fontName), message);
      chrome.test.assertEq('string', typeof(value[0].localizedName), message);
    }));
  },

  function setDefaultFontSize() {
    fs.setDefaultFontSize({
      pixelSize: 22
    }, chrome.test.callbackPass());
  },

  function setDefaultFixedFontSize() {
    fs.setDefaultFixedFontSize({
      pixelSize: 42
    }, chrome.test.callbackPass());
  },

  function setMinimumFontSize() {
    fs.setMinimumFontSize({
      pixelSize: 7
    }, chrome.test.callbackPass());
  },

  function getDefaultFontSize() {
    var expected = 22;
    var message = 'Setting for default font size should be ' + expected;
    fs.getDefaultFontSize({}, expect({pixelSize: expected}, message));
  },

  function getDefaultFontSizeOmitDetails() {
    var expected = 22;
    var message = 'Setting for default font size should be ' + expected;
    fs.getDefaultFontSize(expect({pixelSize: expected}, message));
  },

  function getDefaultFixedFontSize() {
    var expected = 42;
    var message = 'Setting for default fixed font size should be ' + expected;
    fs.getDefaultFixedFontSize({}, expect({pixelSize: expected}, message));
  },

  function getMinimumFontSize() {
    var expected = 7;
    var message = 'Setting for minimum font size should be ' + expected;
    fs.getMinimumFontSize({}, expect({pixelSize: expected}, message));
  }
]);
