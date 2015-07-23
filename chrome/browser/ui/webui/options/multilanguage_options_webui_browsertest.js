// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "chrome/browser/ui/webui/options/' +
    'multilanguage_options_browsertest.h"');

/**
 * Test C++ fixture for Language Options WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function MultilanguageOptionsWebUIBrowserTest() {}

MultilanguageOptionsWebUIBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

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
    testing.Test.prototype.setUp.call(this);

    assertTrue(loadTimeData.getBoolean('enableMultilingualSpellChecker'));
    assertFalse(cr.isMac);
    expectTrue($('spellcheck-language-button').hidden);
    this.expectCurrentlySelected('fr');
  },
};

// Test that opening language options has the correct location.
TEST_F('MultilanguageOptionsWebUIBrowserTest', 'TestOpenLanguageOptions',
       function() {
  expectEquals('chrome://settings-frame/languages', document.location.href);
});

// Verify that the option to enable the spelling service is hidden when
// multilingual spellchecking is enabled.
TEST_F('MultilanguageOptionsWebUIBrowserTest', 'HideSpellingServiceCheckbox',
       function() {
  assertTrue(loadTimeData.getBoolean('enableMultilingualSpellChecker'));
  expectTrue($('spelling-enabled-container').hidden);
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
    testing.Test.prototype.setUp.call(this);

    assertTrue(loadTimeData.getBoolean('enableMultilingualSpellChecker'));
    assertFalse(cr.isMac);
    expectTrue($('spellcheck-language-button').hidden);
    this.expectCurrentlySelected('');
    this.expectRegisteredDictionariesPref('');
  },
};

// Make sure the case where no languages are selected is handled properly.
TEST_F('MultilanguagePreferenceWebUIBrowserTest', 'BlankSpellcheckLanguges',
       function() {
  expectTrue($('language-options-list').selectLanguageByCode('fr'));
  expectFalse($('spellcheck-language-checkbox').checked, 'fr');

  // Add a preference change event listener which ensures that the preference is
  // updated correctly and that 'fr' is the only thing in the dictionary object.
  this.addPreferenceListener('spellcheck.dictionaries', function() {
    expectTrue($('spellcheck-language-checkbox').checked, 'fr');
    this.expectRegisteredDictionariesPref('fr');
    this.expectCurrentlySelected('fr');
    testDone();
  }.bind(this));

  // Click 'fr' and trigger the previously registered event listener.
  $('spellcheck-language-checkbox').click();
});

// Make sure that when no languages are selected, the "Enable spell checking"
// checkbox and "Edit custom spelling dictionary" link are not visible.
TEST_F('MultilanguagePreferenceWebUIBrowserTest', 'DeselectAllLanguages',
       function() {
  expectTrue($('language-options-list').selectLanguageByCode('fr'));
  if ($('enable-spellcheck-container'))
    expectTrue($('enable-spellcheck-container').hidden);
  expectTrue($('edit-dictionary-button').hidden);

  this.addPreferenceListener('spellcheck.dictionaries', function() {
    expectTrue($('spellcheck-language-checkbox').checked, 'fr');
    if ($('enable-spellcheck-container'))
      expectFalse($('enable-spellcheck-container').hidden);
    expectFalse($('edit-dictionary-button').hidden);
    testDone();
  });

  // Click 'fr' and trigger the preference listener.
  $('spellcheck-language-checkbox').click();
});

// ChromeOS never shows the "Enable spell checking" checkbox so don't run the
// last test on ChromeOS.
GEN('#if !defined(OS_CHROMEOS)');

// Make sure that disabling spellchecking disables the language checkboxes.
TEST_F('MultilanguagePreferenceWebUIBrowserTest', 'DisableLanguageCheckboxes',
       function() {
  expectTrue($('language-options-list').selectLanguageByCode('fr'));
  expectFalse($('spellcheck-language-checkbox').disabled);

  this.addPreferenceListener('spellcheck.dictionaries', function() {
    this.addPreferenceListener('browser.enable_spellchecking', function() {
      expectTrue($('spellcheck-language-checkbox').disabled);
      testDone();
    });

    // Click 'Enable spell checking' and trigger the preference listener.
    $('enable-spellcheck').click();
  }.bind(this));

  // Click 'fr' to display the 'Enable spell checking' checkbox.
  $('spellcheck-language-checkbox').click();
});

GEN('#endif  // OS_CHROMEOS');
