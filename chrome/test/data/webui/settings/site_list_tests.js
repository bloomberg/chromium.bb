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
          auto_downloads: [],
          background_sync: [],
          camera: [],
          cookies: [],
          geolocation: [
            {
              embeddingOrigin: 'https://bar-allow.com:443',
              origin: 'https://bar-allow.com:443',
              setting: 'allow',
              source: 'preference',
            },
            {
              embeddingOrigin: 'https://foo-allow.com:443',
              origin: 'https://foo-allow.com:443',
              setting: 'allow',
              source: 'preference',
            },
            {
              embeddingOrigin: 'https://bar-block.com:443',
              origin: 'https://bar-block.com:443',
              setting: 'block',
              source: 'preference',
            },
            {
              embeddingOrigin: 'https://foo-block.com:443',
              origin: 'https://foo-block.com:443',
              setting: 'block',
              source: 'preference',
            },
          ],
          images: [],
          javascript: [],
          keygen: [],
          mic: [],
          notifications: [],
          plugins: [],
          popups: [],
          unsandboxed_plugins: [],
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
       * An example pref with exceptions with origins and patterns from
       * different providers.
       * @type {SiteSettingsPref}
       */
      var prefsMixedProvider = {
        exceptions: {
          geolocation: [
            {
              embeddingOrigin: 'https://[*.]foo.com',
              origin: 'https://[*.]foo.com',
              setting: 'block',
              source: 'policy',
            },
            {
              embeddingOrigin: 'https://bar.foo.com',
              origin: 'https://bar.foo.com',
              setting: 'block',
              source: 'preference',
            },
            {
              embeddingOrigin: 'https://[*.]foo.com',
              origin: 'https://[*.]foo.com',
              setting: 'block',
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
          auto_downloads: [],
          background_sync: [],
          camera: [],
          cookies: [],
          geolocation: [
            {
              origin: 'https://foo.com',
              embeddingOrigin: '*',
              setting: 'allow',
              source: 'preference',
            },
          ],
          images: [],
          javascript: [
            {
              origin: 'https://[*.]foo.com',
              embeddingOrigin: '*',
              setting: 'allow',
              source: 'preference',
            },
          ],
          keygen: [],
          mic: [],
          notifications: [],
          plugins: [],
          popups: [],
          unsandboxed_plugins: [],
        }
      };

      /**
       * An example pref with multiple categories and multiple allow/block
       * state.
       * @type {SiteSettingsPref}
       */
      var prefsVarious = {
        exceptions: {
          auto_downloads: [],
          background_sync: [],
          camera: [],
          cookies: [],
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
          keygen: [],
          mic: [],
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
          plugins: [],
          popups: [],
          unsandboxed_plugins: [],
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

      /**
       * An example Cookies pref with 1 in each of the three categories.
       * @type {SiteSettingsPref}
       */
      var prefsSessionOnly = {
        exceptions: {
          cookies: [
            {
              embeddingOrigin: 'http://foo-block.com',
              origin: 'http://foo-block.com',
              setting: 'block',
              source: 'preference',
            },
            {
              embeddingOrigin: 'http://foo-allow.com',
              origin: 'http://foo-allow.com',
              setting: 'allow',
              source: 'preference',
            },
            {
              embeddingOrigin: 'http://foo-session.com',
              origin: 'http://foo-session.com',
              setting: 'session_only',
              source: 'preference',
            },
          ]
        }
      };

      /**
       * An example Cookies pref with mixed incognito and regular settings.
       * @type {SiteSettingsPref}
       */
      var prefsIncognito = {
        exceptions: {
          cookies: [
            // foo.com is blocked for regular sessions.
            {
              embeddingOrigin: 'http://foo.com',
              incognito: false,
              origin: 'http://foo.com',
              setting: 'block',
              source: 'preference',
            },
            // bar.com is an allowed incognito item without an embedder.
            {
              embeddingOrigin: '',
              incognito: true,
              origin: 'http://bar.com',
              setting: 'allow',
              source: 'preference',
            },
            // foo.com is allowed in incognito (overridden).
            {
              embeddingOrigin: 'http://foo.com',
              incognito: true,
              origin: 'http://foo.com',
              setting: 'allow',
              source: 'preference',
            },

          ]
        }
      };

      /**
       * An example Javascript pref with a chrome-extension:// scheme.
       * @type {SiteSettingsPref}
       */
      var prefsChromeExtension = {
        exceptions: {
          javascript: [
            {
              embeddingOrigin: '',
              origin: 'chrome-extension://cfhgfbfpcbnnbibfphagcjmgjfjmojfa/',
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
        testElement = document.createElement('site-list');
        document.body.appendChild(testElement);
      });

      teardown(function() {
        closeActionMenu();
      });

      /**
       * Fetch the non-hidden menu items from the action menu.
       */
      function getMenuItems() {
        var menu = testElement.$$('dialog[is=cr-action-menu]');
        assertTrue(!!menu);
        return menu.querySelectorAll('button:not([hidden])');
      }

      /**
       * Opens the action menu for a particular element in the list.
       * @param {number} index The index of the child element (which site) to
       *     open the action menu for.
       */
      function openActionMenu(index) {
        var item = testElement.$.listContainer.children[index];
        var dots = item.querySelector('paper-icon-button');
        MockInteractions.tap(dots);
        Polymer.dom.flush();
      }

      /** Closes the action menu. */
      function closeActionMenu() {
        var menu = testElement.$$('dialog[is=cr-action-menu]');
        if (menu.open)
          menu.close();
      }

      /**
       * Asserts the menu looks as expected.
       * @param {Array<string>} items The items expected to show in the menu.
       */
      function assertMenu(items) {
        var menuItems = getMenuItems();
        assertEquals(items.length, menuItems.length);
        for (var i = 0; i < items.length; i++)
          assertEquals(items[i], menuItems[i].textContent.trim());
      }

      /**
       * Configures the test element for a particular category.
       * @param {settings.ContentSettingsTypes} category The category to set up.
       * @param {settings.PermissionValues} subtype Type of list to use.
       * @param {Array<dictionary>} prefs The prefs to use.
       */
      function setUpCategory(category, subtype, prefs) {
        browserProxy.setPrefs(prefs);
        if (category == settings.ALL_SITES) {
          testElement.categorySubtype = settings.INVALID_CATEGORY_SUBTYPE;
          testElement.allSites = true;
        } else {
          testElement.categorySubtype = subtype;
          testElement.allSites = false;
        }
        // Some route is needed, but the actual route doesn't matter.
        testElement.currentRoute = {
          page: 'dummy',
          section: 'privacy',
          subpage: ['site-settings', 'site-settings-category-location'],
        };
        testElement.category = category;
      }

      test('getExceptionList API used', function() {
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.ALLOW, prefsEmpty);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);
            });
      });

      test('Empty list', function() {
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.ALLOW, prefsEmpty);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);

              assertEquals(0, testElement.sites.length);

              assertEquals(
                  settings.PermissionValues.ALLOW, testElement.categorySubtype);

              assertFalse(testElement.$.category.hidden);
            });
      });

      test('initial ALLOW state is correct', function() {
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.ALLOW, prefs);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);

              assertEquals(2, testElement.sites.length);
              assertEquals(prefs.exceptions.geolocation[0].origin,
                  testElement.sites[0].origin);
              assertEquals(prefs.exceptions.geolocation[1].origin,
                  testElement.sites[1].origin);
              assertEquals(
                  settings.PermissionValues.ALLOW, testElement.categorySubtype);
              Polymer.dom.flush();  // Populates action menu.
              openActionMenu(0);
              assertMenu(['Block', 'Remove'], testElement);

              assertFalse(testElement.$.category.hidden);
            });
      });

      test('exceptions are not reordered in non-ALL_SITES', function() {
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.BLOCK, prefsMixedProvider);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);

              assertEquals(3, testElement.sites.length);
              for(var i = 0; i < 3; i++) {
                assertEquals(
                    prefsMixedProvider.exceptions.geolocation[0].origin,
                    testElement.sites[0].origin);
                assertEquals(
                    prefsMixedProvider.exceptions.geolocation[0].source,
                    testElement.sites[0].source);
              }
            });
      });

      test('initial BLOCK state is correct', function() {
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.BLOCK, prefs);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);

              assertEquals(2, testElement.sites.length);
              assertEquals(prefs.exceptions.geolocation[2].origin,
                  testElement.sites[0].origin);
              assertEquals(prefs.exceptions.geolocation[3].origin,
                  testElement.sites[1].origin);
              assertEquals(
                  settings.PermissionValues.BLOCK, testElement.categorySubtype);
              Polymer.dom.flush();  // Populates action menu.
              openActionMenu(0);
              assertMenu(['Allow', 'Remove'], testElement);

              assertFalse(testElement.$.category.hidden);
            });
      });

      test('initial SESSION ONLY state is correct', function() {
        setUpCategory(settings.ContentSettingsTypes.COOKIES,
            settings.PermissionValues.SESSION_ONLY, prefsSessionOnly);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.COOKIES, contentType);

              assertEquals(1, testElement.sites.length);
              assertEquals(prefsSessionOnly.exceptions.cookies[2].origin,
                  testElement.sites[0].origin);

              assertEquals(settings.PermissionValues.SESSION_ONLY,
                  testElement.categorySubtype);
              Polymer.dom.flush();  // Populates action menu.
              openActionMenu(0);
              assertMenu(['Allow', 'Block', 'Remove'], testElement);

              assertFalse(testElement.$.category.hidden);
            });
      });

      test('initial INCOGNITO BLOCK state is correct', function() {
        setUpCategory(settings.ContentSettingsTypes.COOKIES,
            settings.PermissionValues.BLOCK, prefsIncognito);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(settings.ContentSettingsTypes.COOKIES, contentType);

              assertEquals(1, testElement.sites.length);
              assertEquals(prefsIncognito.exceptions.cookies[0].origin,
                  testElement.sites[0].origin);

              assertEquals(settings.PermissionValues.BLOCK,
                  testElement.categorySubtype);
              Polymer.dom.flush();  // Populates action menu.
              openActionMenu(0);
              // 'Clear on exit' is visible as this is not an incognito item.
              assertMenu(['Allow', 'Clear on exit', 'Remove'], testElement);

              // Select 'Remove from menu'.
              var menuItems = getMenuItems();
              assertTrue(!!menuItems);
              MockInteractions.tap(menuItems[2]);
              return browserProxy.whenCalled(
                  'resetCategoryPermissionForOrigin');
            }).then(function(args) {
              assertEquals('http://foo.com', args[0]);
              assertEquals('http://foo.com', args[1]);
              assertEquals(settings.ContentSettingsTypes.COOKIES, args[2]);
              assertFalse(args[3]);  // Incognito.
            });
      });

      test('initial INCOGNITO ALLOW state is correct', function() {
        setUpCategory(settings.ContentSettingsTypes.COOKIES,
            settings.PermissionValues.ALLOW, prefsIncognito);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.COOKIES, contentType);

              assertEquals(2, testElement.sites.length);
              assertEquals(prefsIncognito.exceptions.cookies[1].origin,
                  testElement.sites[0].origin);
              assertEquals(prefsIncognito.exceptions.cookies[2].origin,
                  testElement.sites[1].origin);

              assertEquals(settings.PermissionValues.ALLOW,
                  testElement.categorySubtype);
              Polymer.dom.flush();  // Populates action menu.
              openActionMenu(0);
              // 'Clear on exit' is hidden for incognito items.
              assertMenu(['Block', 'Remove'], testElement);
              closeActionMenu();

              // Select 'Remove' from menu on 'foo.com'.
              openActionMenu(1);
              var menuItems = getMenuItems();
              assertTrue(!!menuItems);
              MockInteractions.tap(menuItems[1]);
              return browserProxy.whenCalled(
                  'resetCategoryPermissionForOrigin');
            }).then(function(args) {
              assertEquals('http://foo.com', args[0]);
              assertEquals('http://foo.com', args[1]);
              assertEquals(settings.ContentSettingsTypes.COOKIES, args[2]);
              assertTrue(args[3]);  // Incognito.
            });
      });

      test('list items shown and clickable when data is present', function() {
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.ALLOW, prefs);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              testElement.enableSiteSettings_ = true;
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);

              // Required for firstItem to be found below.
              Polymer.dom.flush();

              // Validate that the sites gets populated from pre-canned prefs.
              assertEquals(2, testElement.sites.length);
              assertEquals(prefs.exceptions.geolocation[0].origin,
                  testElement.sites[0].origin);
              assertEquals(prefs.exceptions.geolocation[1].origin,
                  testElement.sites[1].origin);
              assertEquals(undefined, testElement.selectedOrigin);

              // Validate that the sites are shown in UI and can be selected.
              var firstItem = testElement.$.listContainer.children[0];
              var clickable = firstItem.querySelector('.middle');
              assertNotEquals(undefined, clickable);
              MockInteractions.tap(clickable);
              assertEquals(prefs.exceptions.geolocation[0].origin,
                  testElement.selectedSite.origin);
            });
      });

      test('Block list open when Allow list is empty', function() {
        // Prefs: One item in Block list, nothing in Allow list.
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.BLOCK, prefsOneDisabled);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);
              assertFalse(testElement.$.category.hidden);
            }).then(function() {
              assertNotEquals(0, testElement.$.listContainer.offsetHeight);
            });
      });

      test('Block list closed when Allow list is not empty', function() {
        // Prefs: Items in both Block and Allow list.
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.BLOCK, prefs);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);
              assertFalse(testElement.$.category.hidden);
              assertEquals(0, testElement.$.listContainer.offsetHeight);
            });
      });

      test('Allow list is always open (Block list empty)', function() {
        // Prefs: One item in Allow list, nothing in Block list.
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.ALLOW, prefsOneEnabled);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);

              assertFalse(testElement.$.category.hidden);
            }).then(function() {
              assertNotEquals(0, testElement.$.listContainer.offsetHeight);
            });
      });

      test('Allow list is always open (Block list non-empty)', function() {
        // Prefs: Items in both Block and Allow list.
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.ALLOW, prefs);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);
              assertFalse(testElement.$.category.hidden);
            }).then(function() {
              assertNotEquals(0, testElement.$.listContainer.offsetHeight);
            });
      });

      test('Block list not hidden when empty', function() {
        // Prefs: One item in Allow list, nothing in Block list.
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.BLOCK, prefsOneEnabled);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);
              assertFalse(testElement.$.category.hidden);
            });
      });

      test('Allow list not hidden when empty', function() {
        // Prefs: One item in Block list, nothing in Allow list.
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.ALLOW, prefsOneDisabled);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);
              assertFalse(testElement.$.category.hidden);
            });
      });

      test('All sites category', function() {
        // Prefs: Multiple and overlapping sites.
        setUpCategory(settings.ALL_SITES, '', prefsVarious);

        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              // Use resolver to ensure asserts bubble up to the framework with
              // meaningful errors.
              var resolver = new PromiseResolver();
              testElement.async(resolver.resolve);
              return resolver.promise.then(function() {
                testElement.enableSiteSettings_ = true;
                // All Sites calls getExceptionList for all categories, starting
                // with Cookies.
                assertEquals(
                    settings.ContentSettingsTypes.COOKIES, contentType);

                // Required for firstItem to be found below.
                Polymer.dom.flush();

                assertFalse(testElement.$.category.hidden);
                // Validate that the sites gets populated from pre-canned prefs.
                assertEquals(3, testElement.sites.length,
                    'If this fails with 5 instead of the expected 3, then ' +
                    'the de-duping of sites is not working for site_list');
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
        setUpCategory(settings.ALL_SITES, '', prefsMixedOriginAndPattern);

        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              // Use resolver to ensure asserts bubble up to the framework with
              // meaningful errors.
              var resolver = new PromiseResolver();
              testElement.async(resolver.resolve);
              return resolver.promise.then(function() {
                testElement.enableSiteSettings_ = true;
                // All Sites calls getExceptionList for all categories, starting
                // with Cookies.
                assertEquals(
                    settings.ContentSettingsTypes.COOKIES, contentType);

                // Required for firstItem to be found below.
                Polymer.dom.flush();

                assertFalse(testElement.$.category.hidden);
                // Validate that the sites gets populated from pre-canned prefs.
                // TODO(dschuyler): de-duping of sites is under discussion, so
                // it is currently disabled. It should be enabled again or this
                // code should be removed.
                assertEquals(2, testElement.sites.length,
                    'If this fails with 1 instead of the expected 2, then ' +
                    'the de-duping of sites has been enabled for site_list.');
                if (testElement.sites.length == 1) {
                  assertEquals(
                      prefsMixedOriginAndPattern.exceptions.
                                                 geolocation[0].
                                                 origin,
                      testElement.sites[0].originForDisplay);
                }

                assertEquals(undefined, testElement.selectedOrigin);
                // Validate that the sites are shown in UI and can be selected.
                var firstItem = testElement.$.listContainer.children[0];
                var clickable = firstItem.querySelector('.middle');
                assertNotEquals(undefined, clickable);
                MockInteractions.tap(clickable);
                if (testElement.sites.length == 1) {
                  assertEquals(
                      prefsMixedOriginAndPattern.exceptions.
                                                 geolocation[0].
                                                 origin,
                      testElement.selectedSite.originForDisplay);
                }
              });
            });
      });

      test('Mixed schemes (present and absent)', function() {
        // Prefs: One item with scheme and one without.
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.ALLOW, prefsMixedSchemes);
        return browserProxy.whenCalled('getExceptionList').then(
            function(contentType) {
              // No further checks needed. If this fails, it will hang the test.
            });
      });

      test('Select menu item', function() {
        // Test for error: "Cannot read property 'origin' of undefined".
        setUpCategory(settings.ContentSettingsTypes.GEOLOCATION,
            settings.PermissionValues.ALLOW, prefs);
        return browserProxy.whenCalled('getExceptionList').then(function(
            contentType) {
          Polymer.dom.flush();
          openActionMenu(0);
          var menuItems = getMenuItems();
          assertTrue(!!menuItems);
          MockInteractions.tap(menuItems[0]);
          return browserProxy.whenCalled('setCategoryPermissionForOrigin');
        });
      });

      test('Chrome Extension scheme', function() {
        setUpCategory(settings.ContentSettingsTypes.JAVASCRIPT,
            settings.PermissionValues.BLOCK, prefsChromeExtension);
        return browserProxy.whenCalled('getExceptionList').then(function(
            contentType) {
          Polymer.dom.flush();
          openActionMenu(0);
          assertMenu(['Allow', 'Remove'], testElement);

          var menuItems = getMenuItems();
          assertTrue(!!menuItems);
          MockInteractions.tap(menuItems[0]);  // Action: Allow.
          return browserProxy.whenCalled('setCategoryPermissionForOrigin');
        }).then(function(args) {
          assertEquals('chrome-extension://cfhgfbfpcbnnbibfphagcjmgjfjmojfa/',
              args[0]);
          assertEquals('', args[1]);
          assertEquals(settings.ContentSettingsTypes.JAVASCRIPT, args[2]);
          assertEquals('allow', args[3]);
        });
      });
    });
  }
  return {
    registerTests: registerTests,
  };
});
