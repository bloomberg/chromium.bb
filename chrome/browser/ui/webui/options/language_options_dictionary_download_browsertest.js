// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['options_browsertest_base.js']);

/**
 * TestFixture for testing messages of dictionary download progress in language
 * options WebUI.
 * @extends {testing.Test}
 * @constructor
 */
function LanguagesOptionsDictionaryDownloadWebUITest() {}

LanguagesOptionsDictionaryDownloadWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /**
   * Browse to languages options.
   */
  browsePreload: 'chrome://settings-frame/languages',

  /**
   * Register a mock dictionary handler.
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['retryDictionaryDownload']);
    this.mockHandler.stubs().retryDictionaryDownload().
        will(callFunction(function() {
          options.LanguageOptions.onDictionaryDownloadBegin('en-US');
        }));
  },

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    // Enable when failure is resolved.
    // AX_ARIA_10: http://crbug.com/570554
    this.accessibilityAuditConfig.ignoreSelectors(
        'unsupportedAriaAttribute',
        '#language-options-list');

    // Enable when failure is resolved.
    // AX_TEXT_04: http://crbug.com/570553
    this.accessibilityAuditConfig.ignoreSelectors(
        'linkWithUnclearPurpose',
        '#languagePage > .content-area > .language-options-header > A');
  },
};

// Verify that dictionary download success does not show, "This language can't
// be used for spellchecking." or "Download failed."
TEST_F('LanguagesOptionsDictionaryDownloadWebUITest',
       'testdictionaryDownloadSuccess',
       function() {
  options.LanguageOptions.onDictionaryDownloadSuccess('en-US');
  expectTrue($('spellcheck-language-message').hidden);
  expectTrue($('language-options-dictionary-downloading-message').hidden);
  expectTrue($('language-options-dictionary-download-failed-message').hidden);
  expectTrue(
      $('language-options-dictionary-download-fail-help-message').hidden);
});

// Verify that dictionary download in progress shows 'Downloading spell check
// language' message.
TEST_F('LanguagesOptionsDictionaryDownloadWebUITest',
       'testdictionaryDownloadProgress',
       function() {
  options.LanguageOptions.onDictionaryDownloadBegin('en-US');
  expectTrue($('spellcheck-language-message').hidden);
  expectFalse($('language-options-dictionary-downloading-message').hidden);
  expectTrue($('language-options-dictionary-download-failed-message').hidden);
  expectTrue(
      $('language-options-dictionary-download-fail-help-message').hidden);
});

// Verify that failure in dictionary download shows 'Dictionary download failed'
// message.
TEST_F('LanguagesOptionsDictionaryDownloadWebUITest',
       'testdictionaryDownloadFailed',
       function() {
  // Clear the failure counter:
  options.LanguageOptions.onDictionaryDownloadSuccess('en-US');

  // First failure shows a short error message.
  options.LanguageOptions.onDictionaryDownloadFailure('en-US');
  expectTrue($('spellcheck-language-message').hidden);
  expectTrue($('language-options-dictionary-downloading-message').hidden);
  expectFalse($('language-options-dictionary-download-failed-message').hidden);
  expectTrue(
      $('language-options-dictionary-download-fail-help-message').hidden);

  // Second and all following failures show a longer error message.
  options.LanguageOptions.onDictionaryDownloadFailure('en-US');
  expectTrue($('spellcheck-language-message').hidden);
  expectTrue($('language-options-dictionary-downloading-message').hidden);
  expectFalse($('language-options-dictionary-download-failed-message').hidden);
  expectFalse(
      $('language-options-dictionary-download-fail-help-message').hidden);

  options.LanguageOptions.onDictionaryDownloadFailure('en-US');
  expectTrue($('spellcheck-language-message').hidden);
  expectTrue($('language-options-dictionary-downloading-message').hidden);
  expectFalse($('language-options-dictionary-download-failed-message').hidden);
  expectFalse(
      $('language-options-dictionary-download-fail-help-message').hidden);
});

// Verify that clicking the retry button calls the handler.
TEST_F('LanguagesOptionsDictionaryDownloadWebUITest',
       'testdictionaryDownloadRetry',
       function() {
  this.mockHandler.expects(once()).retryDictionaryDownload('en-US').
      will(callFunction(function() {
        options.LanguageOptions.onDictionaryDownloadBegin('en-US');
      }));
  options.LanguageOptions.onDictionaryDownloadFailure('en-US');
  $('dictionary-download-retry-button').click();
});
