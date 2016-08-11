// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_main_page', function() {
  /**
   * @implements {SearchManager}
   */
  var TestSearchManager = function() {
    /** @private {boolean} */
    this.matchesFound_ = true;

    /** @private {?settings.SearchRequest} */
    this.searchRequest_ = null;
  }

  TestSearchManager.prototype = {
    /**
     * @param {boolean} matchesFound
     */
    setMatchesFound: function(matchesFound) {
      this.matchesFound_ = matchesFound;
    },

    /** @override */
    search: function(text, page) {
      if (this.searchRequest_ == null || !this.searchRequest_.isSame(text)) {
        this.searchRequest_ = new settings.SearchRequest(text);
        this.searchRequest_.finished = true;
        this.searchRequest_.updateMatches(this.matchesFound_);
        this.searchRequest_.resolver.resolve(this.searchRequest_);
      }
      return this.searchRequest_.resolver.promise;
    },
  };

  function registerTests() {
    var settingsPrefs = null;

    suiteSetup(function() {
      settingsPrefs = document.createElement('settings-prefs');
      return CrSettingsPrefs.initialized;
    });

    suite('SearchTests', function() {
      /** @type {?TestSearchManager} */
      var searchManager = null;

      /** @type {?SettingsMainElement} */
      var settingsMain = null;

      setup(function() {
        searchManager = new TestSearchManager();
        settings.setSearchManagerForTesting(searchManager);
        PolymerTest.clearBody();
        settingsMain = document.createElement('settings-main');
        settingsMain.prefs = settingsPrefs.prefs;
        settingsMain.toolbarSpinnerActive = false;
        document.body.appendChild(settingsMain);
      });

      teardown(function() { settingsMain.remove(); });

      test('no results page shows and hides', function() {
        Polymer.dom.flush();
        var noSearchResults = settingsMain.$.noSearchResults;
        assertTrue(!!noSearchResults);
        assertTrue(noSearchResults.hidden);

        var toggleContainer = settingsMain.$$('#toggleContainer');
        assertTrue(!!toggleContainer);
        assertNotEquals('none', toggleContainer.style.display);

        searchManager.setMatchesFound(false);
        return settingsMain.searchContents('Query1').then(function() {
          assertFalse(noSearchResults.hidden);
          assertEquals('none', toggleContainer.style.display);

          searchManager.setMatchesFound(true);
          return settingsMain.searchContents('Query2');
        }).then(function() {
          assertTrue(noSearchResults.hidden);
        });
      });

      // Ensure that when the user clears the search box, the "no results" page
      // is hidden and the "advanced page toggle" is visible again.
      test('no results page hides on clear', function() {
        Polymer.dom.flush();
        var noSearchResults = settingsMain.$.noSearchResults;
        assertTrue(!!noSearchResults);
        assertTrue(noSearchResults.hidden);

        var toggleContainer = settingsMain.$$('#toggleContainer');
        assertTrue(!!toggleContainer);
        assertNotEquals('none', toggleContainer.style.display);

        searchManager.setMatchesFound(false);
        // Clearing the search box is effectively a search for the empty string.
        return settingsMain.searchContents('').then(function() {
          Polymer.dom.flush();
          assertTrue(noSearchResults.hidden);
          assertNotEquals('none', toggleContainer.style.display);
        });
      });

      test('advanced page restored after search results cleared', function() {
        // Simulating searching while the advanced page is collapsed.
        settingsMain.currentRouteChanged(settings.Route.BASIC);
        Polymer.dom.flush();

        assertFalse(!!settingsMain.$$('settings-advanced-page'));

        searchManager.setMatchesFound(true);
        return settingsMain.searchContents('Query1').then(function() {
          searchManager.setMatchesFound(false);
          return settingsMain.searchContents('');
        }).then(function() {
          Polymer.dom.flush();
          // Checking that it remains hidden after results are cleared.
          assertEquals(
              'none', settingsMain.$$('settings-advanced-page').style.display);
        });
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
