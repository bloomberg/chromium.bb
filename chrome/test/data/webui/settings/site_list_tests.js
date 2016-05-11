// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list. */
cr.define('site_list', function() {
  function registerTests() {
    suite('SiteList', function() {
      /**
       * A site list element created before each test.
       * @type {SiteList}
       */
      var testElement;

      /**
       * The mock proxy object to use during test.
       * @type {TestSiteSettingsPrefsBrowserProxy}
       */
      var browserProxy = null;

      /**
       * An example pref with 2 blocked location items and 2 allowed. This pref
       * is also used for the All Sites category and therefore needs values for
       * all types, even though some might be blank.
       * @type {SiteSettingsPref}
       */
      var prefs = {
        exceptions: {
          media_stream_camera: [],
          cookies: [],
          fullscreen: [],
          geolocation: [
            {
              embeddingOrigin: 'https://foo-allow.com:443',
              origin: 'https://foo-allow.com:443',
              setting: 'allow',
              source: 'preference',
            },
            {
              embeddingOrigin: 'https://bar-allow.com:443',
              origin: 'https://bar-allow.com:443',
              setting: 'allow',
              source: 'preference',
            },
            {
              embeddingOrigin: 'https://foo-block.com:443',
              origin: 'https://foo-block.com:443',
              setting: 'block',
              source: 'preference',
            },
            {
              embeddingOrigin: 'https://bar-block.com:443',
              origin: 'https://bar-block.com:443',
              setting: 'block',
              source: 'preference',
            },
          ],
          images: [],
          javascript: [],
          media_stream_mic: [],
          notifications: [],
          popups: [],
        }
      };

      /**
       * An example pref with mixed schemes (present and absent).
       * @type {SiteSettingsPref}
       */
      var prefsMixedSchemes = {
        exceptions: {
          geolocation: [
            {
              embeddingOrigin: 'https://foo-allow.com',
              origin: 'https://foo-allow.com',
              setting: 'allow',
              source: 'preference',
            },
            {
              embeddingOrigin: 'bar-allow.com',
              origin: 'bar-allow.com',
              setting: 'allow',
              source: 'preference',
            },
          ]
        }
      };

      /**
       * An example pref with mixed origin and pattern.
       * @type {SiteSettingsPref}
       */
      var prefsMixedOriginAndPattern = {
        exceptions: {
          media_stream_camera: [],
          cookies: [],
          fullscreen: [],
          geolocation: [
            {
              origin: 'https://foo.com',
              embeddingOrigin: '*',
              setting: 'allow',
              source: 'preference',
            },
          ],
          javascript: [
            {
              origin: 'https://[*.]foo.com',
              embeddingOrigin: '*',
              setting: 'allow',
              source: 'preference',
            },
          ],
          images: [],
          media_stream_mic: [],
          notifications: [],
          popups: [],
        }
      };

      /**
       * An example pref with multiple categories and multiple allow/block
       * state.
       * @type {SiteSettingsPref}
       */
      var prefsVarious = {
        exceptions: {
          media_stream_camera: [],
          cookies: [],
          fullscreen: [],
          geolocation: [
            {
              embeddingOrigin: 'https://foo.com',
              origin: 'https://foo.com',
              setting: 'allow',
              source: 'preference',
            },
            {
              embeddingOrigin: 'https://bar.com',
              origin: 'https://bar.com',
              setting: 'block',
              source: 'preference',
            },
          ],
          images: [],
          javascript: [],
          media_stream_mic: [],
          notifications: [
            {
              embeddingOrigin: 'https://google.com',
              origin: 'https://google.com',
              setting: 'block',
              source: 'preference',
            },
            {
              embeddingOrigin: 'https://bar.com',
              origin: 'https://bar.com',
              setting: 'block',
              source: 'preference',
            },
            {
              embeddingOrigin: 'https://foo.com',
              origin: 'https://foo.com',
              setting: 'block',
              source: 'preference',
            },
          ],
          popups: [],
        }
      };

      /**
       * An example pref with 1 allowed location item.
       * @type {SiteSettingsPref}
       */
      var prefsOneEnabled = {
        exceptions: {
          geolocation: [
            {
              embeddingOrigin: 'https://foo-allow.com:443',
              origin: 'https://foo-allow.com:443',
              setting: 'allow',
              source: 'preference',
            },
          ]
        }
      };

      /**
       * An example pref with 1 blocked location item.
       * @type {SiteSettingsPref}
       */
      var prefsOneDisabled = {
        exceptions: {
          geolocation: [
            {
              embeddingOrigin: 'https://foo-block.com:443',
              origin: 'https://foo-block.com:443',
              setting: 'block',
              source: 'preference',
            },
          ]
        }
      };

      // Import necessary html before running suite.
      suiteSetup(function() {
        CrSettingsPrefs.setInitialized();
        return PolymerTest.importHtml(
            'chrome://md-settings/site_settings/site_list.html');
      });

      suiteTeardown(function() {
        CrSettingsPrefs.resetForTesting();
      });

      // Initialize a site-list before each test.
      setup(function() {
        browserProxy = new TestSiteSettingsPrefsBrowserProxy();
        settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        testElement = document.createElement('settings-site-list');
        document.body.appendChild(testElement);
      });

      /**
       * Asserts if a menu action is incorrectly hidden.
       * @param {!HTMLElement} parentElement The parent node to start looking
       *     in.
       * @param {string} textForHiddenAction Text content of the only node that
       *     should be hidden.
       */
      function assertMenuActionHidden(parentElement, textForHiddenAction) {
        var listItem = parentElement.$.listContainer.children[0];
        var menuItems =
            listItem.querySelectorAll('paper-menu-button paper-item');
        assertNotEquals(0, menuItems.length);

        var found = false;
        menuItems.forEach(function(item) {
          var text = item.textContent.trim();
          if (text == textForHiddenAction)
            found = true;
          assertEquals(text == textForHiddenAction, item.hidden);
        });
        assertTrue(found);
      }

      /**
       * Configures the test element as a location category.
       * @param {settings.PermissionValues} subtype Type of list to use: ALLOW
       *     or BLOCK.
       * @param Array<dictionary> prefs The prefs to use.
       */
      function setupLocationCategory(subtype, prefs) {
        browserProxy.setPrefs(prefs);
        testElement.categorySubtype = subtype;
        testElement.categoryEnabled = true;
        testElement.allSites = false;
        testElement.currentRoute = {
          page: 'dummy',
          section: 'privacy',
          subpage: ['site-settings', 'site-settings-category-location'],
        };
        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
      }

      /**
       * Configures the test element as the all sites category.
       * @param {dictionary} prefs The prefs to use.
       */
      function setupAllSitesCategory(prefs) {
        browserProxy.setPrefs(prefs);
        testElement.categorySubtype = settings.INVALID_CATEGORY_SUBTYPE;
        testElement.categoryEnabled = true;
        testElement.allSites = true;
        testElement.prefs = prefs;
        testElement.currentRoute = {
          page: 'dummy',
          section: 'privacy',
          subpage: ['site-settings', 'site-settings-category-location'],
        };
        testElement.category = settings.ALL_SITES;
      }

      test('getExceptionList API used', function() {
        setupLocationCategory(settings.PermissionValues.ALLOW, prefsEmpty);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);
            });
      });

      test('Empty list', function() {
        setupLocationCategory(settings.PermissionValues.ALLOW, prefsEmpty);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);

            assertEquals(0, testElement.sites.length);

            assertEquals(
                settings.PermissionValues.ALLOW, testElement.categorySubtype);
            assertEquals('Allow - 0', testElement.$.header.innerText.trim());

            // Site list should not show, no matter what category default is set
            // to.
            assertTrue(testElement.$.category.hidden);
            browserProxy.resetResolver('getExceptionList');
            testElement.categoryEnabled = false;
            return browserProxy.whenCalled('getExceptionList').then(
              function(contentType) {
                assertTrue(testElement.$.category.hidden);
                assertEquals('Exceptions - 0',
                    testElement.$.header.innerText.trim());
            });
          });
      });

      test('initial ALLOW state is correct', function() {
        setupLocationCategory(settings.PermissionValues.ALLOW, prefs);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);

            assertEquals(2, testElement.sites.length);
            assertEquals(prefs.exceptions.geolocation[1].origin,
                testElement.sites[0].origin);
            assertEquals(
                settings.PermissionValues.ALLOW, testElement.categorySubtype);
            Polymer.dom.flush();  // Populates action menu.
            assertMenuActionHidden(testElement, 'Allow');
            assertEquals('Allow - 2', testElement.$.header.innerText.trim());

            // Site list should show, no matter what category default is set to.
            assertFalse(testElement.$.category.hidden);
            browserProxy.resetResolver('getExceptionList');
            testElement.categoryEnabled = false;
            return browserProxy.whenCalled('getExceptionList').then(
              function(contentType) {
                assertFalse(testElement.$.category.hidden);
                assertEquals('Exceptions - 2', testElement.$.header.innerText);
            });
          });
      });

      test('initial BLOCK state is correct', function() {
        setupLocationCategory(settings.PermissionValues.BLOCK, prefs);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);

            assertEquals(2, testElement.sites.length);
            assertEquals(prefs.exceptions.geolocation[3].origin,
                testElement.sites[0].origin);

            assertEquals(
                settings.PermissionValues.BLOCK, testElement.categorySubtype);
            Polymer.dom.flush();  // Populates action menu.
            assertMenuActionHidden(testElement, 'Block');
            assertEquals('Block - 2', testElement.$.header.innerText.trim());

            // Site list should only show when category default is enabled.
            assertFalse(testElement.$.category.hidden);
            browserProxy.resetResolver('getExceptionList');
            testElement.categoryEnabled = false;
            return browserProxy.whenCalled('getExceptionList').then(
              function(contentType) {
                assertTrue(testElement.$.category.hidden);
            });
          });
      });

      test('list items shown and clickable when data is present', function() {
        setupLocationCategory(settings.PermissionValues.ALLOW, prefs);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);

            // Required for firstItem to be found below.
            Polymer.dom.flush();

            // Validate that the sites gets populated from pre-canned prefs.
            assertEquals(2, testElement.sites.length);
            assertEquals(prefs.exceptions.geolocation[1].origin,
                testElement.sites[0].origin);
            assertEquals(undefined, testElement.selectedOrigin);

            // Validate that the sites are shown in UI and can be selected.
            var firstItem = testElement.$.listContainer.children[0];
            var clickable = firstItem.querySelector('.middle');
            assertNotEquals(undefined, clickable);
            MockInteractions.tap(clickable);
            assertEquals(prefs.exceptions.geolocation[1].origin,
                testElement.selectedSite.origin);
        });
      });

      test('Block list open when Allow list is empty', function() {
        // Prefs: One item in Block list, nothing in Allow list.
        setupLocationCategory(settings.PermissionValues.BLOCK,
                              prefsOneDisabled);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);

            assertFalse(testElement.$.category.hidden);
            assertTrue(testElement.$.category.opened);
        }).then(function() {
            assertNotEquals(0, testElement.$.listContainer.offsetHeight);
        });
      });

      test('Block list closed when Allow list is not empty', function() {
        // Prefs: Items in both Block and Allow list.
        setupLocationCategory(settings.PermissionValues.BLOCK, prefs);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);

            assertFalse(testElement.$.category.hidden);
            assertFalse(testElement.$.category.opened);
            assertEquals(0, testElement.$.listContainer.offsetHeight);
        });
      });

      test('Allow list is always open (Block list empty)', function() {
        // Prefs: One item in Allow list, nothing in Block list.
        setupLocationCategory(
            settings.PermissionValues.ALLOW, prefsOneEnabled);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);

            assertFalse(testElement.$.category.hidden);
            assertTrue(testElement.$.category.opened);
        }).then(function() {
            assertNotEquals(0, testElement.$.listContainer.offsetHeight);
        });
      });

      test('Allow list is always open (Block list non-empty)', function() {
        // Prefs: Items in both Block and Allow list.
        setupLocationCategory(settings.PermissionValues.ALLOW, prefs);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);

            assertFalse(testElement.$.category.hidden);
            assertTrue(testElement.$.category.opened);
        }).then(function() {
            assertNotEquals(0, testElement.$.listContainer.offsetHeight);
        });
      });

      test('Block list hidden when empty', function() {
        // Prefs: One item in Allow list, nothing in Block list.
        setupLocationCategory(
            settings.PermissionValues.BLOCK, prefsOneEnabled);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);

            assertTrue(testElement.$.category.hidden);
        });
      });

      test('Allow list hidden when empty', function() {
        // Prefs: One item in Block list, nothing in Allow list.
        setupLocationCategory(settings.PermissionValues.ALLOW,
                              prefsOneDisabled);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);

            assertTrue(testElement.$.category.hidden);
        });
      });

      test('All sites category', function() {
        // Prefs: Multiple and overlapping sites.
        setupAllSitesCategory(prefsVarious);

        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            // Use resolver to ensure asserts bubble up to the framework with
            // meaningful errors.
            var resolver = new PromiseResolver();
            testElement.async(resolver.resolve);
            return resolver.promise.then(function() {
              // All Sites calls getExceptionList for all categories, starting
              // with Cookies.
              assertEquals(settings.ContentSettingsTypes.COOKIES, contentType);

              // Required for firstItem to be found below.
              Polymer.dom.flush();

              assertTrue(testElement.$.category.opened);
              assertFalse(testElement.$.category.hidden);
              // Validate that the sites gets populated from pre-canned prefs.
              assertEquals(3, testElement.sites.length,
                  'If this fails with 5 instead of the expected 3, then the ' +
                  'de-duping of sites is not working for site_list');
              assertEquals(prefsVarious.exceptions.geolocation[1].origin,
                  testElement.sites[0].origin);
              assertEquals(prefsVarious.exceptions.geolocation[0].origin,
                  testElement.sites[1].origin);
              assertEquals(prefsVarious.exceptions.notifications[0].origin,
                  testElement.sites[2].origin);
              assertEquals(undefined, testElement.selectedOrigin);

              // Validate that the sites are shown in UI and can be selected.
              var firstItem = testElement.$.listContainer.children[1];
              var clickable = firstItem.querySelector('.middle');
              assertNotEquals(undefined, clickable);
              MockInteractions.tap(clickable);
              assertEquals(prefsVarious.exceptions.geolocation[0].origin,
                  testElement.selectedSite.origin);
            });
          });
      });

      test('All sites mixed pattern and origin', function() {
        // Prefs: One site, represented as origin and pattern.
        setupAllSitesCategory(prefsMixedOriginAndPattern);

        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            // Use resolver to ensure asserts bubble up to the framework with
            // meaningful errors.
            var resolver = new PromiseResolver();
            testElement.async(resolver.resolve);
            return resolver.promise.then(function() {
              // All Sites calls getExceptionList for all categories, starting
              // with Cookies.
              assertEquals(settings.ContentSettingsTypes.COOKIES, contentType);

              // Required for firstItem to be found below.
              Polymer.dom.flush();

              assertTrue(testElement.$.category.opened);
              assertFalse(testElement.$.category.hidden);
              // Validate that the sites gets populated from pre-canned prefs.
              assertEquals(1, testElement.sites.length,
                  'If this fails with 2 instead of the expected 1, then the ' +
                  'de-duping of sites is not working for site_list');
              assertEquals(
                  prefsMixedOriginAndPattern.exceptions.geolocation[0].origin,
                  testElement.sites[0].originForDisplay);

              assertEquals(undefined, testElement.selectedOrigin);
              // Validate that the sites are shown in UI and can be selected.
              var firstItem = testElement.$.listContainer.children[0];
              var clickable = firstItem.querySelector('.middle');
              assertNotEquals(undefined, clickable);
              MockInteractions.tap(clickable);
              assertEquals(
                  prefsMixedOriginAndPattern.exceptions.geolocation[0].origin,
                  testElement.selectedSite.originForDisplay);
            });
          });
      });

      test('Mixed schemes (present and absent)', function() {
        // Prefs: One item with scheme and one without.
        setupLocationCategory(settings.PermissionValues.ALLOW,
                              prefsMixedSchemes);
        return browserProxy.whenCalled('getExceptionList').then(
          function(contentType) {
            // No further checks needed. If this fails, it will hang the test.
        });
      });
    });
  }
  return {
    registerTests: registerTests,
  };
});
