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

    suite('MainPageTests', function() {
      /** @type {?TestSearchManager} */
      var searchManager = null;

      /** @type {?SettingsMainElement} */
      var settingsMain = null;

      setup(function() {
        settings.navigateTo(settings.Route.BASIC);
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

      /**
       * Asserts the visibility of the basic and advanced pages after exiting
       * search mode.
       * @param {string} Expected 'display' value for the basic page.
       * @param {string} Expected 'display' value for the advanced page.
       * @return {!Promise}
       */
      function assertPageVisibilityAfterSearch(
          expectedBasic, expectedAdvanced) {
        searchManager.setMatchesFound(true);
        return settingsMain.searchContents('Query1').then(function() {
          searchManager.setMatchesFound(false);
          return settingsMain.searchContents('');
        }).then(function() {
          Polymer.dom.flush();
          assertEquals(
              expectedBasic,
              settingsMain.$$('settings-basic-page').style.display);
          assertEquals(
              expectedAdvanced,
              settingsMain.$$('settings-advanced-page').style.display);
        });
      }

      test('exiting search mode, advanced collapsed', function() {
        // Simulating searching while the advanced page is collapsed.
        settingsMain.currentRouteChanged(settings.Route.BASIC);
        Polymer.dom.flush();
        return assertPageVisibilityAfterSearch('', 'none');
      });

      // Ensure that clearing the search results restores both "basic" and
      // "advanced" page, when the search has been initiated from a subpage
      // whose parent is the "advanced" page.
      test('exiting search mode, advanced expanded', function() {
        settings.navigateTo(settings.Route.SITE_SETTINGS);
        Polymer.dom.flush();
        return assertPageVisibilityAfterSearch('', '');
      });

      // Ensure that searching, then entering a subpage, then going back
      // lands the user in a page where both basic and advanced sections are
      // visible, because the page is still in search mode.
      test('returning from subpage to search results', function() {
        settings.navigateTo(settings.Route.BASIC);
        Polymer.dom.flush();

        searchManager.setMatchesFound(true);
        return settingsMain.searchContents('Query1').then(function() {
          // Simulate navigating into a subpage.
          settings.navigateTo(settings.Route.SEARCH_ENGINES);
          settingsMain.$$('settings-basic-page').fire('subpage-expand');
          Polymer.dom.flush();

          // Simulate clicking the left arrow to go back to the search results.
          settingsMain.currentRouteChanged(settings.Route.BASIC);
          Polymer.dom.flush();
          assertEquals(
              '', settingsMain.$$('settings-basic-page').style.display);
          assertEquals(
              '', settingsMain.$$('settings-advanced-page').style.display);
        });
      });

      test('can collapse advanced on advanced section route', function() {
        settings.navigateTo(settings.Route.PRIVACY);
        Polymer.dom.flush();

        var advancedToggle = settingsMain.$$('#advancedToggle');
        assertTrue(!!advancedToggle);

        MockInteractions.tap(advancedToggle);
        Polymer.dom.flush();

        assertFalse(settingsMain.showPages_.advanced);
      });

      test('navigating to a basic page does not collapse advanced', function() {
        settings.navigateTo(settings.Route.PRIVACY);
        Polymer.dom.flush();

        var advancedToggle = settingsMain.$$('#advancedToggle');
        assertTrue(!!advancedToggle);

        settings.navigateTo(settings.Route.PEOPLE);
        Polymer.dom.flush();

        assertTrue(settingsMain.showPages_.advanced);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
