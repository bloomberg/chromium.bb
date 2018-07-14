// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An example pref with multiple categories and multiple allow/block
 * state.
 * @type {SiteSettingsPref}
 */
let prefsVarious;

suite('AllSites', function() {
  /**
   * A site list element created before each test.
   * @type {SiteList}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  // Initialize a site-list before each test.
  setup(function() {
    prefsVarious = test_util.createSiteSettingsPrefs([], [
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.GEOLOCATION,
          [
            test_util.createRawSiteException('https://foo.com'),
            test_util.createRawSiteException('https://bar.com', {
              setting: settings.ContentSetting.BLOCK,
            })
          ]),
      test_util.createContentSettingTypeToValuePair(
          settings.ContentSettingsTypes.NOTIFICATIONS,
          [
            test_util.createRawSiteException('https://google.com', {
              setting: settings.ContentSetting.BLOCK,
            }),
            test_util.createRawSiteException('https://bar.com', {
              setting: settings.ContentSetting.BLOCK,
            }),
            test_util.createRawSiteException('https://foo.com', {
              setting: settings.ContentSetting.BLOCK,
            }),
          ])
    ]);

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('all-sites');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    settings.resetRouteForTesting();
  });

  /**
   * Configures the test element.
   * @param {Array<dictionary>} prefs The prefs to use.
   */
  function setUpCategory(prefs) {
    browserProxy.setPrefs(prefs);
    // Some route is needed, but the actual route doesn't matter.
    testElement.currentRoute = {
      page: 'dummy',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-location'],
    };
  }

  test('All sites list populated', function() {
    setUpCategory(prefsVarious);
    testElement.populateList_();
    return browserProxy.whenCalled('getAllSites').then(() => {
      // Use resolver to ensure that the list container is populated.
      const resolver = new PromiseResolver();
      testElement.async(resolver.resolve);
      return resolver.promise.then(() => {
        assertEquals(3, testElement.siteGroupList.length);

        // Flush to be sure list container is populated.
        Polymer.dom.flush();
        const siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(3, siteEntries.length);
      });
    });
  });

  test('search query filters list', function() {
    const SEARCH_QUERY = 'foo';
    setUpCategory(prefsVarious);
    testElement.populateList_();
    return browserProxy.whenCalled('getAllSites')
        .then(() => {
          // Flush to be sure list container is populated.
          Polymer.dom.flush();
          const siteEntries =
              testElement.$.listContainer.querySelectorAll('site-entry');
          assertEquals(3, siteEntries.length);

          testElement.searchQuery_ = SEARCH_QUERY;
        })
        .then(() => {
          Polymer.dom.flush();
          const siteEntries =
              testElement.$.listContainer.querySelectorAll('site-entry');
          const hiddenSiteEntries = Polymer.dom(testElement.root)
                                        .querySelectorAll('site-entry[hidden]');
          assertEquals(1, siteEntries.length - hiddenSiteEntries.length);

          for (let i = 0; i < siteEntries; ++i) {
            const entry = siteEntries[i];
            if (!hiddenSiteEntries.includes(entry)) {
              assertTrue(entry.siteGroup.origins.some((origin) => {
                return origin.includes(SEARCH_QUERY);
              }));
            }
          }
        });
  });

  test('can be sorted by name', function() {
    setUpCategory(prefsVarious);
    testElement.populateList_();
    return browserProxy.whenCalled('getAllSites').then(() => {
      Polymer.dom.flush();
      const siteEntries =
          testElement.$.listContainer.querySelectorAll('site-entry');

      // TODO(https://crbug.com/835712): When there is more than one sort
      // method, check that the all sites list can be sorted by name from an
      // existing all sites list that is out of order. Currently this is not
      // possible as the all sites list will always initially be sorted by the
      // default sort method, and the default sort method is by name.
      assertEquals(3, siteEntries.length);
      assertEquals('bar.com', siteEntries[0].$.displayName.innerText.trim());
      assertEquals('foo.com', siteEntries[1].$.displayName.innerText.trim());
      assertEquals('google.com', siteEntries[2].$.displayName.innerText.trim());
    });
  });
});
