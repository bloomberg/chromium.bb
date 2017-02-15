// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_main_page', function() {
  /**
   * Extending TestBrowserProxy even though SearchManager is not a browser proxy
   * itself. Essentially TestBrowserProxy can act as a "proxy" for any external
   * dependency, not just "browser proxies" (and maybe should be renamed to
   * TestProxy).
   *
   * @implements {SearchManager}
   * @extends {settings.TestBrowserProxy}
   */
  var TestSearchManager = function() {
    settings.TestBrowserProxy.call(this, [
      'search',
    ]);

    /** @private {boolean} */
    this.matchesFound_ = true;

    /** @private {?settings.SearchRequest} */
    this.searchRequest_ = null;
  }

  TestSearchManager.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /**
     * @param {boolean} matchesFound
     */
    setMatchesFound: function(matchesFound) {
      this.matchesFound_ = matchesFound;
    },

    /** @override */
    search: function(text, page) {
      this.methodCalled('search', text);

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

      test('searchContents() triggers SearchManager', function() {
        Polymer.dom.flush();

        var expectedQuery1 = 'foo';
        var expectedQuery2 = 'bar';
        var expectedQuery3 = '';

        return settingsMain.searchContents(expectedQuery1).then(function() {
          return searchManager.whenCalled('search');
        }).then(function(query) {
          assertEquals(expectedQuery1, query);

          searchManager.resetResolver('search');
          return settingsMain.searchContents(expectedQuery2);
        }).then(function() {
          return searchManager.whenCalled('search');
        }).then(function(query) {
          assertEquals(expectedQuery2, query);

          searchManager.resetResolver('search');
          return settingsMain.searchContents(expectedQuery3);
        }).then(function() {
          return searchManager.whenCalled('search');
        }).then(function(query) {
          assertEquals(expectedQuery3, query);
        });
      });

      /** @return {!HTMLElement} */
      function getToggleContainer() {
        var page = settingsMain.$$('settings-basic-page');
        assertTrue(!!page);
        var toggleContainer = page.$$('#toggleContainer');
        assertTrue(!!toggleContainer);
        return toggleContainer;
      }

      /**
       * Asserts that the Advanced toggle container exists in the combined
       * settings page and asserts whether it should be visible.
       * @param {boolean} expectedVisible
       */
      function assertToggleContainerVisible(expectedVisible) {
        var toggleContainer = getToggleContainer();
        if (expectedVisible)
          assertNotEquals('none', toggleContainer.style.display);
        else
          assertEquals('none', toggleContainer.style.display);
      }

      test('no results page shows and hides', function() {
        Polymer.dom.flush();
        var noSearchResults = settingsMain.$.noSearchResults;
        assertTrue(!!noSearchResults);
        assertTrue(noSearchResults.hidden);

        assertToggleContainerVisible(true);

        searchManager.setMatchesFound(false);
        return settingsMain.searchContents('Query1').then(function() {
          assertFalse(noSearchResults.hidden);
          assertToggleContainerVisible(false);

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

        assertToggleContainerVisible(true);

        searchManager.setMatchesFound(false);
        // Clearing the search box is effectively a search for the empty string.
        return settingsMain.searchContents('').then(function() {
          Polymer.dom.flush();
          assertTrue(noSearchResults.hidden);
          assertToggleContainerVisible(true);
        });
      });

      /**
       * Asserts the visibility of the basic and advanced pages.
       * @param {string} Expected 'display' value for the basic page.
       * @param {string} Expected 'display' value for the advanced page.
       */
      function assertPageVisibility(expectedBasic, expectedAdvanced) {
        Polymer.dom.flush();
        var page = settingsMain.$$('settings-basic-page');
        assertEquals(
            expectedBasic, page.$$('#basicPage').style.display);
        assertEquals(
            expectedAdvanced,
            page.$$('#advancedPageTemplate').get().style.display);
      }

      // TODO(michaelpg): It would be better not to drill into
      // settings-basic-page. If search should indeed only work in Settings
      // (as opposed to Advanced), perhaps some of this logic should be
      // delegated to settings-basic-page now instead of settings-main.

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
          assertPageVisibility(expectedBasic, expectedAdvanced);
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
          settings.navigateTo(settings.Route.BASIC);
          assertPageVisibility('', '');
        });
      });

      // TODO(michaelpg): Move these to a new test for settings-basic-page.
      test('can collapse advanced on advanced section route', function() {
        settings.navigateTo(settings.Route.PRIVACY);
        Polymer.dom.flush();

        var advancedToggle =
            getToggleContainer().querySelector('#advancedToggle');
        assertTrue(!!advancedToggle);

        MockInteractions.tap(advancedToggle);
        Polymer.dom.flush();

        assertPageVisibility('', 'none');
      });

      test('navigating to a basic page does not collapse advanced', function() {
        settings.navigateTo(settings.Route.PRIVACY);
        Polymer.dom.flush();

        assertToggleContainerVisible(true);

        settings.navigateTo(settings.Route.PEOPLE);
        Polymer.dom.flush();

        assertPageVisibility('', '');
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
