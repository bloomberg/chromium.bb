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
       * An example pref with 2 blocked location items and 2 allowed.
       * @type {SiteSettingsPref}
       */
      var prefs = {
        exceptions: {
          location: [
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
          ]
        }
      };

      /**
       * An example pref with mixed schemes (present and absent).
       * @type {SiteSettingsPref}
       */
      var prefsMixedSchemes = {
        exceptions: {
          location: [
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
       * An example pref with multiple categories and multiple allow/block
       * state.
       * @type {SiteSettingsPref}
       */
      var prefsVarious = {
        exceptions: {
          location: [
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
          ]
        }
      };

      /**
       * An example pref with 1 allowed location item.
       * @type {SiteSettingsPref}
       */
      var prefsOneEnabled = {
        exceptions: {
          location: [
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
          location: [
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
        cr.exportPath('settings_test');
        settings_test.siteListNotifyForTest = true;

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
       * Returns a promise that resolves once the site list has been updated.
       * @param {function()} action is executed after the listener is set up.
       * @return {!Promise} a Promise fulfilled when the selected item changes.
       */
      function runAndResolveWhenSitesChanged(action) {
        return new Promise(function(resolve, reject) {
          var handler = function() {
            testElement.removeEventListener('sites-changed', handler);
            resolve();
          };
          testElement.addEventListener('sites-changed', handler);
          action();
        });
      }

      /**
       * Asserts if a menu action is incorrectly hidden.
       * @param {!HTMLElement} parentElement The parent node to start looking
       *     in.
       * @param {string} textForHiddenAction Text content of the only node that
       *     should be hidden.
       */
      function assertMenuActionHidden(parentElement, textForHiddenAction) {
        var actions = parentElement.$.listContainer.items;
        for (var i = 0; i < actions.length; ++i) {
          var content = actions[i].textContent.trim();
          if (content == textForHiddenAction)
            assertTrue(actions[i].hidden);
          else
            assertFalse(actions[i].hidden);
        }
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
        return runAndResolveWhenSitesChanged(function() {
          setupLocationCategory(settings.PermissionValues.ALLOW, prefsEmpty);
        }).then(function() {
          assertEquals(0, testElement.sites.length);

          assertTrue(testElement.isAllowList_());
          assertFalse(testElement.showSiteList_(testElement.sites, true));
          assertFalse(testElement.showSiteList_(testElement.sites, false));
          assertEquals('Allow - 0', testElement.computeSiteListHeader_(
              testElement.sites, true));
          assertEquals('Exceptions - 0', testElement.computeSiteListHeader_(
              testElement.sites, false));
        }.bind(this));
      });

      test('initial ALLOW state is correct', function() {
        return runAndResolveWhenSitesChanged(function() {
          setupLocationCategory(settings.PermissionValues.ALLOW, prefs);
        }).then(function() {
          assertEquals(2, testElement.sites.length);
          assertEquals('https://bar-allow.com:443', testElement.sites[0]);
          assertTrue(testElement.isAllowList_());
          assertMenuActionHidden(testElement, 'Allow');
          // Site list should show, no matter what category default is set to.
          assertTrue(testElement.showSiteList_(testElement.sites, true));
          assertTrue(testElement.showSiteList_(testElement.sites, false));
          assertEquals('Exceptions - 2', testElement.computeSiteListHeader_(
              testElement.sites, false));
          assertEquals('Allow - 2', testElement.computeSiteListHeader_(
              testElement.sites, true));
        }.bind(this));
      });

      test('initial BLOCK state is correct', function() {
        return runAndResolveWhenSitesChanged(function() {
          setupLocationCategory(settings.PermissionValues.BLOCK, prefs);
        }).then(function() {
          assertEquals(2, testElement.sites.length);
          assertEquals('https://bar-block.com:443', testElement.sites[0]);

          assertFalse(testElement.isAllowList_());
          assertMenuActionHidden(testElement, 'Block');
          // Site list should only show when category default is enabled.
          assertFalse(testElement.showSiteList_(testElement.sites, false));
          assertTrue(testElement.showSiteList_(testElement.sites, true));
          assertEquals('Block - 2', testElement.computeSiteListHeader_(
              testElement.sites, true));
        }.bind(this));
      });

      test('list items shown and clickable when data is present', function() {
        return runAndResolveWhenSitesChanged(function() {
          setupLocationCategory(settings.PermissionValues.ALLOW, prefs);
        }).then(function() {
          // Required for firstItem to be found below.
          Polymer.dom.flush();

          // Validate that the sites gets populated from pre-canned prefs.
          assertEquals(2, testElement.sites.length);
          assertEquals('https://bar-allow.com:443', testElement.sites[0]);
          assertEquals(undefined, testElement.selectedOrigin);

          // Validate that the sites are shown in UI and can be selected.
          var firstItem = testElement.$.listContainer.items[0];
          var clickable = firstItem.querySelector('.flex paper-item');
          assertNotEquals(undefined, clickable);
          MockInteractions.tap(clickable);
          assertEquals('https://bar-allow.com:443', testElement.selectedOrigin);
        }.bind(this));
      });

      test('Block list open when Allow list is empty', function() {
        return runAndResolveWhenSitesChanged(function() {
          // Prefs: One item in Block list, nothing in Allow list.
          setupLocationCategory(settings.PermissionValues.BLOCK,
                                prefsOneDisabled);
        }).then(function() {
          assertFalse(testElement.$.category.hidden);
          assertTrue(testElement.$.category.opened);
          assertNotEquals(0, testElement.$.listContainer.offsetHeight);
        }.bind(this));
      });

      test('Block list closed when Allow list is not empty', function() {
        return runAndResolveWhenSitesChanged(function() {
          // Prefs: Items in both Block and Allow list.
          setupLocationCategory(settings.PermissionValues.BLOCK, prefs);
        }).then(function() {
          assertFalse(testElement.$.category.hidden);
          assertFalse(testElement.$.category.opened);
          assertEquals(0, testElement.$.listContainer.offsetHeight);
        }.bind(this));
      });

      test('Allow list is always open (Block list empty)', function() {
        return runAndResolveWhenSitesChanged(function() {
          // Prefs: One item in Allow list, nothing in Block list.
          setupLocationCategory(
              settings.PermissionValues.ALLOW, prefsOneEnabled);
        }).then(function() {
          assertFalse(testElement.$.category.hidden);
          assertTrue(testElement.$.category.opened);
          assertNotEquals(0, testElement.$.listContainer.offsetHeight);
        }.bind(this));
      });

      test('Allow list is always open (Block list non-empty)', function() {
        return runAndResolveWhenSitesChanged(function() {
          // Prefs: Items in both Block and Allow list.
          setupLocationCategory(settings.PermissionValues.ALLOW, prefs);
        }).then(function() {
          assertFalse(testElement.$.category.hidden);
          assertTrue(testElement.$.category.opened);
          assertNotEquals(0, testElement.$.listContainer.offsetHeight);
        }.bind(this));
      });

      test('Block list hidden when empty', function() {
        return runAndResolveWhenSitesChanged(function() {
          // Prefs: One item in Allow list, nothing in Block list.
          setupLocationCategory(
              settings.PermissionValues.BLOCK, prefsOneEnabled);
        }).then(function() {
          assertTrue(testElement.$.category.hidden);
        }.bind(this));
      });

      test('Allow list hidden when empty', function() {
        return runAndResolveWhenSitesChanged(function() {
          // Prefs: One item in Block list, nothing in Allow list.
          setupLocationCategory(settings.PermissionValues.ALLOW,
                                prefsOneDisabled);
        }).then(function() {
          assertTrue(testElement.$.category.hidden);
        }.bind(this));
      });

      test('All sites category', function() {
        return runAndResolveWhenSitesChanged(function() {
          // Prefs: Multiple and overlapping sites.
          setupAllSitesCategory(prefsVarious);
        }).then(function() {
          // Required for firstItem to be found below.
          Polymer.dom.flush();

          assertFalse(testElement.$.category.hidden);
          // Validate that the sites gets populated from pre-canned prefs.
          assertEquals(3, testElement.sites.length);
          assertEquals('https://bar.com', testElement.sites[0]);
          assertEquals('https://foo.com', testElement.sites[1]);
          assertEquals('https://google.com', testElement.sites[2]);
          assertEquals(undefined, testElement.selectedOrigin);

          // Validate that the sites are shown in UI and can be selected.
          var firstItem = testElement.$.listContainer.items[1];
          var clickable = firstItem.querySelector('.flex paper-item');
          assertNotEquals(undefined, clickable);
          MockInteractions.tap(clickable);
          assertEquals('https://foo.com', testElement.selectedOrigin);
        }.bind(this));
      });

      test('Mixed schemes (present and absent)', function() {
        return runAndResolveWhenSitesChanged(function() {
          // Prefs: One item with scheme and one without.
          setupLocationCategory(settings.PermissionValues.ALLOW,
                                prefsMixedSchemes);
        }).then(function() {
          // No further checks needed. If this fails, it will hang the test.
        }.bind(this));
      });
    });
  }
  return {
    registerTests: registerTests,
  };
});
