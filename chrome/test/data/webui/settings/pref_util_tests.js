// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for Settings.PrefUtil. */
cr.define('settings_prefUtil', function() {
  function registerTests() {
    suite('PrefUtil', function() {
      // Import source before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://md-settings/prefs/pref_util.html');
      });

      var global = function() { return this; }();
      var origTraceAssertionsForTesting = global.traceAssertionsForTesting;

      /**
       * @param {function()} fn Function that should throw.
       * @param {string} message Message to log if function does not throw.
       */
      var expectThrows = function(fn, message) {
        // Temporarily disable printing of stack traces on assert failures.
        global.traceAssertionsForTesting = false;

        try {
          fn();
          // Must be expect, because assert would get caught.
          expectNotReached(message);
        } catch (e) {}

        global.traceAssertionsForTesting = origTraceAssertionsForTesting;
      };

      // Tests that the given value is converted to the expected value, for a
      // given prefType.
      var expectStringToPrefValue = function(value, prefType, expectedValue) {
        var pref = /** @type {PrefObject} */({type: prefType});
        expectEquals(expectedValue,
                     Settings.PrefUtil.stringToPrefValue(value, pref));
      };

      test('stringToPrefValue', function testStringToPrefValue() {
        expectStringToPrefValue(
            'true', chrome.settingsPrivate.PrefType.BOOLEAN, true);

        expectStringToPrefValue(
            'false', chrome.settingsPrivate.PrefType.BOOLEAN, false);

        expectStringToPrefValue(
            '42', chrome.settingsPrivate.PrefType.NUMBER, 42);

        expectStringToPrefValue(
            'Foo Bar', chrome.settingsPrivate.PrefType.STRING, 'Foo Bar');

        var url = 'http://example.com';
        expectStringToPrefValue(url, chrome.settingsPrivate.PrefType.URL, url);

        expectThrows(function() {
          expectStringToPrefValue(
              '[1, 2]', chrome.settingsPrivate.PrefType.LIST, '');
        }, 'List prefs should not be converted.');

        expectThrows(function() {
          expectStringToPrefValue(
              '{foo: 1}', chrome.settingsPrivate.PrefType.DICTIONARY, '');
        }, 'Dictionary prefs should not be converted.');
      });

      // Tests that the pref value is converted to the expected string, for a
      // given prefType.
      var expectPrefToString = function(prefType, prefValue, expectedValue) {
        var pref = /** @type {PrefObject} */({
          type: prefType,
          value: prefValue,
        });
        expectEquals(expectedValue, Settings.PrefUtil.prefToString(pref));
      };

      test('prefToString', function testPrefToString() {
        expectPrefToString(chrome.settingsPrivate.PrefType.BOOLEAN,
                           false, 'false');

        expectPrefToString(chrome.settingsPrivate.PrefType.NUMBER, 42, '42');

        expectPrefToString(chrome.settingsPrivate.PrefType.STRING,
                           'Foo Bar', 'Foo Bar');

        var url = 'http://example.com';
        expectPrefToString(chrome.settingsPrivate.PrefType.URL, url, url);

        expectThrows(function() {
          expectPrefToString(chrome.settingsPrivate.PrefType.LIST,
                             [1, 2], null);
        }, 'List prefs should not be handled.');

        expectThrows(function() {
          expectPrefToString(chrome.settingsPrivate.PrefType.DICTIONARY,
                             {foo: 1}, null);
        }, 'Dictionary prefs should not be handled.');
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
