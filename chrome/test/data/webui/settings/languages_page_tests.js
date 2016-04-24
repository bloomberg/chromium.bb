// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_languages_page', function() {
  suite('settings-languages-singleton', function() {
    var languageSettingsPrivate;
    var languageHelper;
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
      fakePrefs.push({
        key: 'settings.language.preload_engines',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us::eng,' +
               '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us:dvorak:eng',
      });
      fakePrefs.push({
        key: 'settings.language.enabled_extension_imes',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: '',
      });
    }

    suiteSetup(function() {
      CrSettingsPrefs.deferInitialization = true;
      settingsPrefs = document.createElement('settings-prefs');
      assertTrue(!!settingsPrefs);
      var fakeApi = new settings.FakeSettingsPrivate(fakePrefs);
      settingsPrefs.initializeForTesting(fakeApi);

      languageSettingsPrivate = new settings.FakeLanguageSettingsPrivate();
      languageSettings.languageSettingsPrivateApiForTest =
          languageSettingsPrivate;


      languageSettingsPrivate.setSettingsPrefs(settingsPrefs);
      LanguageHelperImpl.instance_ = undefined;
      languageHelper = LanguageHelperImpl.getInstance();
      return languageHelper.whenReady();
    });

    test('languages model', function() {
      for (var i = 0; i < languageSettingsPrivate.languages.length;
           i++) {
        assertEquals(languageSettingsPrivate.languages[i].code,
                     languageHelper.languages.supported[i].code);
      }
      assertEquals(2, languageHelper.languages.enabled.length);
      assertEquals('en-US',
                   languageHelper.languages.enabled[0].language.code);
      assertEquals('sw',
                   languageHelper.languages.enabled[1].language.code);
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

    if (cr.isChromeOS) {
      test('modifying input methods', function() {
        assertEquals(2, languageHelper.languages.inputMethods.enabled.length);
        var inputMethods = languageHelper.getInputMethodsForLanguage('en-US');
        assertEquals(2, inputMethods.length);

        var dvorak =
            '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us:dvorak:eng';
        languageHelper.removeInputMethod(dvorak);
        assertEquals(1, languageHelper.languages.inputMethods.enabled.length);

        // TODO(michaelpg): Test other modifications.
      });
    }
  });
});
