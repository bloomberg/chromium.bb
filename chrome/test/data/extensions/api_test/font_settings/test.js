// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Font settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.FontSettings

var fs = chrome.experimental.fontSettings;
var CONTROLLABLE_BY_THIS_EXTENSION = 'controllable_by_this_extension';

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  // This test may fail on Windows if the font is not installed on the
  // system. See crbug.com/122303
  function setPerScriptFont() {
    var script = 'Hang';
    var genericFamily = 'standard';
    var fontName = 'Verdana';

    chrome.test.listenOnce(fs.onFontChanged, function(details) {
      chrome.test.assertEq({
        script: script,
        genericFamily: genericFamily,
        fontName: fontName,
        levelOfControl: 'controlled_by_this_extension'
      }, details);
    });

    fs.setFont({
      script: script,
      genericFamily: genericFamily,
      fontName: fontName
    }, chrome.test.callbackPass());
  },

  function getPerScriptFontName() {
    var expected = 'Verdana';
    var message = 'Setting for Hangul standard font should be ' + expected;

    fs.getFont({
      script: 'Hang',
      genericFamily: 'standard'
    }, expect({fontName: expected}, message));
  },

  // This test may fail on Windows if the font is not installed on
  // the system. See crbug.com/122303
  function setGlobalFontName() {
    var fontName = 'Tahoma';
    var genericFamily = 'sansserif';

    chrome.test.listenOnce(fs.onFontChanged, function(details) {
      chrome.test.assertEq({
        genericFamily: genericFamily,
        fontName: fontName,
        levelOfControl: 'controlled_by_this_extension'
      }, details);
    });

    fs.setFont({
      genericFamily: 'sansserif',
      fontName: 'Tahoma'
    }, chrome.test.callbackPass());
  },

  function getGlobalFontName() {
    var expected = 'Tahoma';
    var message =
        'Setting for global sansserif font should be ' + expected;

    fs.getFont({
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
    var pixelSize = 22;
    chrome.test.listenOnce(fs.onDefaultFontSizeChanged, function(details) {
      chrome.test.assertEq({
        pixelSize: pixelSize,
        levelOfControl: 'controlled_by_this_extension'
      }, details);
    });

    fs.setDefaultFontSize({
      pixelSize: pixelSize
    }, chrome.test.callbackPass());
  },

  function setDefaultFixedFontSize() {
    var pixelSize = 42;
    chrome.test.listenOnce(fs.onDefaultFixedFontSizeChanged, function(details) {
      chrome.test.assertEq({
        pixelSize: pixelSize,
        levelOfControl: 'controlled_by_this_extension'
      }, details);
    });

    fs.setDefaultFixedFontSize({
      pixelSize: pixelSize
    }, chrome.test.callbackPass());
  },

  function setMinimumFontSize() {
    var pixelSize = 7;
    chrome.test.listenOnce(fs.onMinimumFontSizeChanged, function(details) {
      chrome.test.assertEq({
        pixelSize: pixelSize,
        levelOfControl: 'controlled_by_this_extension'
      }, details);
    });

    fs.setMinimumFontSize({
      pixelSize: pixelSize
    }, chrome.test.callbackPass());
  },

  function setDefaultCharacterSet() {
    var charset = 'GBK';
    chrome.test.listenOnce(fs.onDefaultCharacterSetChanged, function(details) {
      chrome.test.assertEq({
        charset: charset,
        levelOfControl: 'controlled_by_this_extension'
      }, details);
    });

    fs.setDefaultCharacterSet({
      charset: charset
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
  },

  function getDefaultCharacterSet() {
    var expected = 'GBK';
    var message = 'Setting for default character set should be ' + expected;
    fs.getDefaultCharacterSet(expect({charset: expected}, message));
  },

  // This test may fail on Windows if the font is not installed on the
  // system. See crbug.com/122303
  function clearPerScriptFont() {
    var script = 'Hang';
    var genericFamily = 'standard';
    var fontName = 'Tahoma';

    chrome.test.listenOnce(fs.onFontChanged, function(details) {
      chrome.test.assertEq({
        script: script,
        genericFamily: genericFamily,
        fontName: fontName,
        levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION
      }, details);
    });

    fs.clearFont({
      script: script,
      genericFamily: genericFamily,
    }, chrome.test.callbackPass());
  },

  // This test may fail on Windows if the font is not installed on the
  // system. See crbug.com/122303
  function clearGlobalFont() {
    var genericFamily = 'sansserif';
    var fontName = 'Arial';

    chrome.test.listenOnce(fs.onFontChanged, function(details) {
      chrome.test.assertEq({
        genericFamily: genericFamily,
        fontName: fontName,
        levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION
      }, details);
    });

    fs.clearFont({
      genericFamily: genericFamily,
    }, chrome.test.callbackPass());
  },

  function clearDefaultFontSize() {
    var pixelSize = 16;
    chrome.test.listenOnce(fs.onDefaultFontSizeChanged, function(details) {
      chrome.test.assertEq({
        pixelSize: pixelSize,
        levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION
      }, details);
    });

    fs.clearDefaultFontSize({}, chrome.test.callbackPass());
  },

  function clearDefaultFixedFontSize() {
    var pixelSize = 14;
    chrome.test.listenOnce(fs.onDefaultFixedFontSizeChanged, function(details) {
      chrome.test.assertEq({
        pixelSize: pixelSize,
        levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION
      }, details);
    });

    fs.clearDefaultFixedFontSize({}, chrome.test.callbackPass());
  },

  function clearMinimumFontSize() {
    var pixelSize = 8;
    chrome.test.listenOnce(fs.onMinimumFontSizeChanged, function(details) {
      chrome.test.assertEq({
        pixelSize: pixelSize,
        levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION
      }, details);
    });

    fs.clearMinimumFontSize({}, chrome.test.callbackPass());
  },

  function clearDefaultCharacterSet() {
    var charset = 'Shift_JIS';
    chrome.test.listenOnce(fs.onDefaultCharacterSetChanged, function(details) {
      chrome.test.assertEq({
        charset: charset,
        levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION
      }, details);
    });

    fs.clearDefaultCharacterSet({}, chrome.test.callbackPass());
  },
]);
