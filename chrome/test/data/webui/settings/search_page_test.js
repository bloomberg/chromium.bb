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
        var listboxElement = page.$$('paper-listbox');

        return browserProxy.whenCalled('getSearchEnginesList').then(function() {
          assertEquals(0, listboxElement.selected);

          // Simulate a user initiated change of the default search engine.
          listboxElement.selected = 1;
          return browserProxy.whenCalled('setDefaultSearchEngine');
        }).then(function() {
          assertEquals(1, listboxElement.selected);

          // Simulate a change that happened in a different tab.
          var searchEnginesInfo = generateSearchEngineInfo();
          searchEnginesInfo.defaults[0].default = false;
          searchEnginesInfo.defaults[1].default = false;
          searchEnginesInfo.defaults[2].default = true;

          browserProxy.resetResolver('setDefaultSearchEngine');
          cr.webUIListenerCallback('search-engines-changed', searchEnginesInfo);
          assertEquals(2, listboxElement.selected);

          browserProxy.whenCalled('setDefaultSearchEngine').then(function() {
            // Since the change happened in a different tab, there should be no
            // new call to |setDefaultSearchEngine|.
            assertNotReached('Should not call setDefaultSearchEngine again');
          });
        });
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
