// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-languages-page. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
// SettingsPageBrowserTest fixture.
GEN_INCLUDE([ROOT_PATH +
             'chrome/test/data/webui/settings/settings_page_browsertest.js']);

var languageSettings = languageSettings || {};

/**
 * Test class for settings-languages-singleton.
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsLanguagesSingletonBrowserTest() {
}

SettingsLanguagesSingletonBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/languages_page/languages.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    '../fake_chrome_event.js',
    'fake_language_settings_private.js',
    'fake_settings_private.js'
  ]),

  /** @type {LanguageSettingsPrivate} */
  languageSettingsPrivateApi: undefined,

  /** @override */
  runAccessibilityChecks: false,

  /** @override */
  preLoad: function() {
    PolymerTest.prototype.preLoad.call(this);
    this.languageSettingsPrivateApi =
        new settings.FakeLanguageSettingsPrivate();
    cr.exportPath('languageSettings').languageSettingsPrivateApiForTest =
        this.languageSettingsPrivateApi;
  },
};

// Tests settings-languages-singleton.
TEST_F('SettingsLanguagesSingletonBrowserTest', 'LanguagesSingleton',
       function() {
  var settingsPrefs;
  var fakePrefs = [{
    key: 'intl.app_locale',
    type: chrome.settingsPrivate.PrefType.STRING,
    value: 'en-US',
  }, {
    key: 'intl.accept_languages',
    type: chrome.settingsPrivate.PrefType.STRING,
    value: 'en-US,sw',
  }, {
    key: 'spellcheck.dictionaries',
    type: chrome.settingsPrivate.PrefType.LIST,
    value: ['en-US'],
  }, {
    key: 'translate_blocked_languages',
    type: chrome.settingsPrivate.PrefType.LIST,
    value: ['en-US'],
  }];
  if (cr.isChromeOS) {
    fakePrefs.push({
      key: 'settings.language.preferred_languages',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'en-US,sw',
    });
  }

  var self = this;
  suite('LanguagesSingleton', function() {
    var languageHelper;

    suiteSetup(function() {
      CrSettingsPrefs.deferInitialization = true;
      settingsPrefs = document.createElement('settings-prefs');
      assertTrue(!!settingsPrefs);
      var fakeApi = new settings.FakeSettingsPrivate(fakePrefs);
      settingsPrefs.initializeForTesting(fakeApi);

      self.languageSettingsPrivateApi.setSettingsPrefs(settingsPrefs);
      languageHelper = LanguageHelperImpl.getInstance();
      return languageHelper.initialized;
    });

    test('languages model', function() {
      for (var i = 0; i < self.languageSettingsPrivateApi.languages.length;
           i++) {
        assertEquals(self.languageSettingsPrivateApi.languages[i].code,
                     languageHelper.languages.supportedLanguages[i].code);
      }
      assertEquals('en-US',
                   languageHelper.languages.enabledLanguages[0].language.code);
      assertEquals('sw',
                   languageHelper.languages.enabledLanguages[1].language.code);
      assertEquals('en', languageHelper.languages.translateTarget);

      // TODO(michaelpg): Test other aspects of the model.
    });

    test('modifying languages', function() {
      assertTrue(languageHelper.isLanguageEnabled('en-US'));
      assertTrue(languageHelper.isLanguageEnabled('sw'));
      assertFalse(languageHelper.isLanguageEnabled('en-CA'));

      languageHelper.enableLanguage('en-CA');
      assertTrue(languageHelper.isLanguageEnabled('en-CA'));
      languageHelper.disableLanguage('sw');
      assertFalse(languageHelper.isLanguageEnabled('sw'));

      // TODO(michaelpg): Test other modifications.
    });
  });

  mocha.run();
});

/**
 * Test class for settings-languages-page.
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsLanguagesPageBrowserTest() {
}

SettingsLanguagesPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/advanced',

  /** @override */
  preLoad: function() {
    SettingsPageBrowserTest.prototype.preLoad.call(this);
    settingsHidePagesByDefaultForTest = true;
  },
};

// May time out on debug builders and memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_LanguagesPage DISABLED_LanguagesPage');
GEN('#else');
GEN('#define MAYBE_LanguagesPage LanguagesPage');
GEN('#endif');

// Runs languages page tests.
TEST_F('SettingsLanguagesPageBrowserTest', 'MAYBE_LanguagesPage', function() {
  suite('languages page', function() {
    testing.Test.disableAnimationsAndTransitions();

    var advanced = this.getPage('advanced');

    suiteSetup(function() {
      advanced.set('pageVisibility.languages', true);
      Polymer.dom.flush();

      return LanguageHelperImpl.getInstance().initialized;
    });

    test('languages page', function(done) {
      var languagesSection = this.getSection(advanced, 'languages');
      assertTrue(!!languagesSection);
      var languagesPage =
          languagesSection.querySelector('settings-languages-page');
      assertTrue(!!languagesPage);

      var manageLanguagesButton =
          languagesPage.$.languagesCollapse.querySelector('.list-button');
      MockInteractions.tap(manageLanguagesButton);
      assertTrue(!!languagesPage.$$('settings-manage-languages-page'));

      // TODO(michaelpg): figure out why setTimeout is necessary, only after
      // opening the first subpage.
      setTimeout(function() {
        var languageButton = languagesPage.$.languagesCollapse.querySelector(
            '.list-item paper-icon-button[icon=settings]');
        assertTrue(!!languageButton);
        MockInteractions.tap(languageButton);

        assertTrue(!!languagesPage.root.querySelector(
            'settings-language-detail-page'));

        var spellCheckCollapse = languagesPage.$.spellCheckCollapse;
        assertEquals(cr.isMac, !spellCheckCollapse);
        if (!cr.isMac) {
          var spellCheckButton = spellCheckCollapse.querySelector(
              '.list-button');
          MockInteractions.tap(spellCheckButton);
          assertTrue(!!languagesPage.$$('settings-edit-dictionary-page'));
        }
        done();
      }.bind(this));
    }.bind(this));
  }.bind(this));

  // TODO(michaelpg): Test more aspects of the languages UI.

  // Run all registered tests.
  mocha.run();
});
