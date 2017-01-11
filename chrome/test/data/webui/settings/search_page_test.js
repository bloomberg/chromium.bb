// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_search_page', function() {
  function generateSearchEngineInfo() {
    var searchEngines0 = settings_search.createSampleSearchEngine(
        true, false, false);
    searchEngines0.default = true;
    var searchEngines1 = settings_search.createSampleSearchEngine(
        true, false, false);
    searchEngines1.default = false;
    var searchEngines2 = settings_search.createSampleSearchEngine(
        true, false, false);
    searchEngines2.default = false;

    return {
      defaults: [searchEngines0, searchEngines1, searchEngines2],
      others: [],
      extensions: [],
    };
  }

  function registerTests() {
    suite('SearchPageTests', function() {
      /** @type {?SettingsSearchPageElement} */
      var page = null;

      var browserProxy = null;

      setup(function() {
        browserProxy = new settings_search.TestSearchEnginesBrowserProxy();
        browserProxy.setSearchEnginesInfo(generateSearchEngineInfo());
        settings.SearchEnginesBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-search-page');
        document.body.appendChild(page);
      });

      teardown(function() { page.remove(); });

      // Tests that the page is querying and displaying search engine info on
      // startup.
      test('Initialization', function() {
        var selectElement = page.$$('select');

        return browserProxy.whenCalled('getSearchEnginesList').then(function() {
          Polymer.dom.flush();
          assertEquals(0, selectElement.selectedIndex);

          // Simulate a user initiated change of the default search engine.
          selectElement.selectedIndex = 1;
          selectElement.dispatchEvent(new CustomEvent('change'));
          return browserProxy.whenCalled('setDefaultSearchEngine');
        }).then(function() {
          assertEquals(1, selectElement.selectedIndex);

          // Simulate a change that happened in a different tab.
          var searchEnginesInfo = generateSearchEngineInfo();
          searchEnginesInfo.defaults[0].default = false;
          searchEnginesInfo.defaults[1].default = false;
          searchEnginesInfo.defaults[2].default = true;

          browserProxy.resetResolver('setDefaultSearchEngine');
          cr.webUIListenerCallback('search-engines-changed', searchEnginesInfo);
          Polymer.dom.flush();
          assertEquals(2, selectElement.selectedIndex);

          browserProxy.whenCalled('setDefaultSearchEngine').then(function() {
            // Since the change happened in a different tab, there should be no
            // new call to |setDefaultSearchEngine|.
            assertNotReached('Should not call setDefaultSearchEngine again');
          });
        });
      });

      test('ControlledByExtension', function() {
        return browserProxy.whenCalled('getSearchEnginesList').then(function() {
          var selectElement = page.$$('select');
          assertFalse(selectElement.disabled);
          assertFalse(!!page.$$('extension-controlled-indicator'));

          page.prefs = {
            default_search_provider: {
              enabled: {
                controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
                controlledByName: 'fake extension name',
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                extensionId: 'fake extension id',
                extensionCanBeDisabled: true,
                value: true,
              },
            },
          };
          Polymer.dom.flush();

          assertTrue(selectElement.disabled);
          assertTrue(!!page.$$('extension-controlled-indicator'));
        });
      });

      // Tests the UI when Hotword 'alwaysOn' is true.
      test('HotwordAlwaysOn', function() {
        return browserProxy.whenCalled('getHotwordInfo').then(function() {
          Polymer.dom.flush();
          assertTrue(page.hotwordInfo_.allowed);
          assertTrue(page.hotwordInfo_.alwaysOn);
          assertFalse(page.hotwordInfo_.enabled);
          assertFalse(browserProxy.hotwordSearchEnabled);
          assertFalse(page.hotwordSearchEnablePref_.value);

          var control = page.$$('#hotwordSearchEnable');
          assertTrue(!!control);
          assertFalse(control.disabled);
          assertFalse(control.checked);
          MockInteractions.tap(control.$.control);
          Polymer.dom.flush();
          return browserProxy.whenCalled('setHotwordSearchEnabled');
        }).then(function() {
          assertTrue(browserProxy.hotwordSearchEnabled);
        });
      });

      // Tests the UI when Hotword 'alwaysOn' is false.
      test('HotwordNotAlwaysOn', function() {
        return browserProxy.whenCalled('getHotwordInfo').then(function() {
          browserProxy.setHotwordInfo({
            allowed: true,
            enabled: false,
            alwaysOn: false,
            errorMessage: '',
            userName: '',
            historyEnabled: false,
          });
          Polymer.dom.flush();
          assertTrue(page.hotwordInfo_.allowed);
          assertFalse(page.hotwordInfo_.alwaysOn);
          assertFalse(page.hotwordInfo_.enabled);

          var control = page.$$('#hotwordSearchEnable');
          assertTrue(!!control);
          assertFalse(control.disabled);
          assertFalse(control.checked);
          MockInteractions.tap(control.$.control);
          Polymer.dom.flush();
          return browserProxy.whenCalled('setHotwordSearchEnabled');
        }).then(function() {
          assertTrue(browserProxy.hotwordSearchEnabled);
        });
      });

      test('UpdateGoogleNowOnPrefChange', function() {
        return browserProxy.whenCalled('getGoogleNowAvailability').then(
            function() {
          Polymer.dom.flush();
          assertTrue(page.googleNowAvailable_);

          var control = page.$$('#googleNowEnable');
          assertTrue(!!control);
          assertFalse(control.disabled);
          assertFalse(control.checked);

          page.prefs = {
            google_now_launcher: {
              enabled: {
                type: chrome.settingsPrivate.PrefType.BOOLEAN,
                value: true,
              }
            }
          };
          Polymer.dom.flush();
          assertFalse(control.disabled);
          assertTrue(control.checked);
        });
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
