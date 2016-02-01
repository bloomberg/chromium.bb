// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['options_browsertest_base.js']);
GEN('#include "chrome/browser/ui/webui/options/' +
    'multilanguage_options_browsertest.h"');

/**
 * Test C++ fixture for Language Options WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function MultilanguageOptionsWebUIBrowserTest() {}

MultilanguageOptionsWebUIBrowserTest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /** @override */
  browsePreload: 'chrome://settings-frame/languages',

  /** @override */
  typedefCppFixture: 'MultilanguageOptionsBrowserTest',

  /** @override */
  accessibilityIssuesAreErrors: true,

  /** @param {string} expected Sorted currently selected languages. */
  expectCurrentlySelected: function(expected) {
    var languages = LanguageOptions.getInstance().spellCheckLanguages_;
    expectEquals(expected, Object.keys(languages).sort().join());
  },

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    assertFalse(cr.isMac);
    expectFalse($('edit-custom-dictionary-button').hidden);
    this.expectEnableSpellcheckCheckboxVisible();
    this.expectCurrentlySelected('fr');

    var requiredOwnedAriaRoleMissingSelectors = [
      '#default-search-engine-list',
      '#other-search-engine-list',
    ];

    // Enable when failure is resolved.
    // AX_ARIA_08: http://crbug.com/559320
    this.accessibilityAuditConfig.ignoreSelectors(
        'requiredOwnedAriaRoleMissing',
        requiredOwnedAriaRoleMissingSelectors);

    // Enable when failure is resolved.
    // AX_ARIA_10: http://crbug.com/559266
    this.accessibilityAuditConfig.ignoreSelectors(
        'unsupportedAriaAttribute',
        '#language-options-list');

    // Enable when failure is resolved.
    // AX_TEXT_04: http://crbug.com/559271
    this.accessibilityAuditConfig.ignoreSelectors(
        'linkWithUnclearPurpose',
        '#languagePage > .content-area > .language-options-header > A');
  },

  /** @override */
  tearDown: function() {
    testing.Test.prototype.tearDown.call(this);
    this.expectEnableSpellcheckCheckboxVisible();
  },

  /** Make sure the 'Enable spell checking' checkbox is visible. */
  expectEnableSpellcheckCheckboxVisible: function() {
    if ($('enable-spellcheck-container'))
      expectFalse($('enable-spellcheck-container').hidden);
  },
};

// Test that opening language options has the correct location.
TEST_F('MultilanguageOptionsWebUIBrowserTest', 'TestOpenLanguageOptions',
       function() {
  expectEquals('chrome://settings-frame/languages', document.location.href);
});

// Test that only certain languages can be selected and used for spellchecking.
// prefs::kLanguagePreferredLanguages/prefs::kAcceptLanguages is set to
// 'fr,es,de,en' and prefs::kSpellCheckDictionaries is just 'fr'
TEST_F('MultilanguageOptionsWebUIBrowserTest', 'ChangeSpellcheckLanguages',
       function() {
  expectTrue($('language-options-list').selectLanguageByCode('es'));
  expectFalse($('spellcheck-language-checkbox').checked, 'es');

  // Click 'es' and ensure that it gets checked and 'fr' stays checked.
  $('spellcheck-language-checkbox').click();
  expectTrue($('spellcheck-language-checkbox').checked, 'es');
  expectTrue($('language-options-list').selectLanguageByCode('fr'));
  expectTrue($('spellcheck-language-checkbox').checked, 'fr');
  this.expectCurrentlySelected('es,fr');

  // Click 'fr' and ensure that it gets unchecked and 'es' stays checked.
  $('spellcheck-language-checkbox').click();
  expectFalse($('spellcheck-language-checkbox').checked, 'fr');
  $('language-options-list').selectLanguageByCode('es');
  expectTrue($('spellcheck-language-checkbox').checked, 'es');
  this.expectCurrentlySelected('es');
});

// Make sure 'am' cannot be selected as a language and 'fr' stays selected.
TEST_F('MultilanguageOptionsWebUIBrowserTest', 'NotAcceptLanguage', function() {
  expectFalse($('language-options-list').selectLanguageByCode('am'));
  expectTrue($('language-options-list').selectLanguageByCode('fr'));
  expectTrue($('spellcheck-language-checkbox').checked, 'fr');
  this.expectCurrentlySelected('fr');
});

// Make sure 'en' cannot be used as a language and 'fr' stays selected.
TEST_F('MultilanguageOptionsWebUIBrowserTest', 'UnusableLanguage', function() {
  expectTrue($('language-options-list').selectLanguageByCode('en'));
  expectTrue($('spellcheck-language-checkbox-container').hidden);
  expectFalse($('spellcheck-language-checkbox').checked, 'en');
  this.expectCurrentlySelected('fr');
});

/**
 * Test C++ fixture for Language Options WebUI testing.
 * @constructor
 * @extends {MultilanguageOptionsWebUIBrowserTest}
 */
function MultilanguagePreferenceWebUIBrowserTest() {}

MultilanguagePreferenceWebUIBrowserTest.prototype = {
  __proto__: MultilanguageOptionsWebUIBrowserTest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('ClearSpellcheckDictionaries();');
  },

  /** @override */
  isAsync: true,

  /**
   * @param {string} expected Sorted languages in the kSpellCheckDictionaries
   * preference.
   */
  expectRegisteredDictionariesPref: function(expected) {
    var registeredPrefs =
        options.Preferences.getInstance().registeredPreferences_;
    expectEquals(expected,
        registeredPrefs['spellcheck.dictionaries'].orig.value.sort().join());
  },

  /**
   * Watch for a change to the preference |pref| and then call |callback|.
   * @param {string} pref The preference to listen for a change on.
   * @param {function} callback The function to run after a listener event.
   */
  addPreferenceListener: function(pref, callback) {
    options.Preferences.getInstance().addEventListener(pref, callback);
  },

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    assertFalse(cr.isMac);
    expectTrue($('edit-custom-dictionary-button').hidden);
    this.expectEnableSpellcheckCheckboxVisible();
    this.expectCurrentlySelected('');
    this.expectRegisteredDictionariesPref('');

    // Enable when failure is resolved.
    // AX_ARIA_10: http://crbug.com/559266
    this.accessibilityAuditConfig.ignoreSelectors(
        'unsupportedAriaAttribute',
        '#language-options-list');

    // Enable when failure is resolved.
    // AX_TEXT_04: http://crbug.com/559271
    this.accessibilityAuditConfig.ignoreSelectors(
        'linkWithUnclearPurpose',
        '#languagePage > .content-area > .language-options-header > A');

    // Enable when failure is resolved.
    // AX_FOCUS_01: http://crbug.com/570046
    this.accessibilityAuditConfig.ignoreSelectors(
        'focusableElementNotVisibleAndNotAriaHidden',
        '#offer-to-translate-in-this-language');
  },
};

// Make sure the case where no languages are selected is handled properly.
TEST_F('MultilanguagePreferenceWebUIBrowserTest', 'SelectFromBlank',
       function() {
  expectTrue($('language-options-list').selectLanguageByCode('fr'));
  expectFalse($('spellcheck-language-checkbox').checked, 'fr');
  expectTrue($('edit-custom-dictionary-button').hidden);

  // Add a preference change event listener which ensures that the preference is
  // updated correctly and that 'fr' is the only thing in the dictionary object.
  this.addPreferenceListener('spellcheck.dictionaries', function() {
    expectTrue($('spellcheck-language-checkbox').checked, 'fr');
    this.expectRegisteredDictionariesPref('fr');
    this.expectCurrentlySelected('fr');
    expectFalse($('edit-custom-dictionary-button').hidden);
    testDone();
  }.bind(this));

  // Click 'fr' and trigger the preference listener.
  $('spellcheck-language-checkbox').click();
});
