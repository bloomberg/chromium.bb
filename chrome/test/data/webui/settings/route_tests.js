// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('route', function() {
  /**
   * Returns a new promise that resolves after a window 'popstate' event.
   * @return {!Promise}
   */
  function whenPopState(causeEvent) {
    var promise = new Promise(function(resolve) {
      window.addEventListener('popstate', function callback() {
        window.removeEventListener('popstate', callback);
        resolve();
      });
    });

    causeEvent();
    return promise;
  }

  /**
   * Tests a specific navigation situation.
   * @param {!settings.Route} previousRoute
   * @param {!settings.Route} currentRoute
   * @param {!settings.Route} expectedNavigatePreviousResult
   * @return {!Promise}
   */
  function testNavigateBackUsesHistory(previousRoute, currentRoute,
                                       expectedNavigatePreviousResult) {
    settings.navigateTo(previousRoute);
    settings.navigateTo(currentRoute);

    return whenPopState(function() {
      settings.navigateToPreviousRoute();
    }).then(function() {
      assertEquals(expectedNavigatePreviousResult,
                   settings.getCurrentRoute());
    });
  };

  test('tree structure', function() {
    // Set up root page routes.
    var BASIC = new settings.Route('/');
    assertEquals(0, BASIC.depth);

    var ADVANCED = new settings.Route('/advanced');
    assertFalse(ADVANCED.isSubpage());
    assertEquals(0, ADVANCED.depth);

    // Test a section route.
    var PRIVACY = ADVANCED.createSection('/privacy', 'privacy');
    assertEquals(ADVANCED, PRIVACY.parent);
    assertEquals(1, PRIVACY.depth);
    assertFalse(PRIVACY.isSubpage());
    assertFalse(BASIC.contains(PRIVACY));
    assertTrue(ADVANCED.contains(PRIVACY));
    assertTrue(PRIVACY.contains(PRIVACY));
    assertFalse(PRIVACY.contains(ADVANCED));

    // Test a subpage route.
    var SITE_SETTINGS = PRIVACY.createChild('/siteSettings');
    assertEquals('/siteSettings', SITE_SETTINGS.path);
    assertEquals(PRIVACY, SITE_SETTINGS.parent);
    assertEquals(2, SITE_SETTINGS.depth);
    assertFalse(!!SITE_SETTINGS.dialog);
    assertTrue(SITE_SETTINGS.isSubpage());
    assertEquals('privacy', SITE_SETTINGS.section);
    assertFalse(BASIC.contains(SITE_SETTINGS));
    assertTrue(ADVANCED.contains(SITE_SETTINGS));
    assertTrue(PRIVACY.contains(SITE_SETTINGS));

    // Test a sub-subpage route.
    var SITE_SETTINGS_ALL = SITE_SETTINGS.createChild('all');
    assertEquals('/siteSettings/all', SITE_SETTINGS_ALL.path);
    assertEquals(SITE_SETTINGS, SITE_SETTINGS_ALL.parent);
    assertEquals(3, SITE_SETTINGS_ALL.depth);
    assertTrue(SITE_SETTINGS_ALL.isSubpage());

    // Test a non-subpage child of ADVANCED.
    var CLEAR_BROWSER_DATA = ADVANCED.createChild('/clearBrowserData');
    assertFalse(CLEAR_BROWSER_DATA.isSubpage());
    assertEquals('', CLEAR_BROWSER_DATA.section);
  });

  test('no duplicate routes', function() {
    var paths = new Set();
    Object.values(settings.Route).forEach(function(route) {
      assertFalse(paths.has(route.path), route.path);
      paths.add(route.path);
    });
  });

  test('navigate back to parent previous route', function() {
    return testNavigateBackUsesHistory(
        settings.Route.BASIC,
        settings.Route.PEOPLE,
        settings.Route.BASIC);
  });

  test('navigate back to non-ancestor shallower route', function() {
    return testNavigateBackUsesHistory(
        settings.Route.ADVANCED,
        settings.Route.PEOPLE,
        settings.Route.ADVANCED);
  });

  test('navigate back to sibling route', function() {
    return testNavigateBackUsesHistory(
        settings.Route.APPEARANCE,
        settings.Route.PEOPLE,
        settings.Route.APPEARANCE);
  });

  test('navigate back to parent when previous route is deeper', function() {
    settings.navigateTo(settings.Route.SYNC);
    settings.navigateTo(settings.Route.PEOPLE);
    settings.navigateToPreviousRoute();
    assertEquals(settings.Route.BASIC, settings.getCurrentRoute());
  });

  test('navigate back to BASIC when going back from root pages', function() {
    settings.navigateTo(settings.Route.PEOPLE);
    settings.navigateTo(settings.Route.ADVANCED);
    settings.navigateToPreviousRoute();
    assertEquals(settings.Route.BASIC, settings.getCurrentRoute());
  });

  test('navigateTo respects removeSearch optional parameter', function() {
    var params = new URLSearchParams('search=foo');
    settings.navigateTo(settings.Route.BASIC, params);
    assertEquals(params.toString(), settings.getQueryParameters().toString());

    settings.navigateTo(
        settings.Route.SITE_SETTINGS, null, /* removeSearch */ false);
    assertEquals(params.toString(), settings.getQueryParameters().toString());

    settings.navigateTo(
        settings.Route.SEARCH_ENGINES, null, /* removeSearch */ true);
    assertEquals('', settings.getQueryParameters().toString());
  });

  test('popstate flag works', function() {
    settings.navigateTo(settings.Route.BASIC);
    assertFalse(settings.lastRouteChangeWasPopstate());

    settings.navigateTo(settings.Route.PEOPLE);
    assertFalse(settings.lastRouteChangeWasPopstate());

    return whenPopState(function() {
      window.history.back();
    }).then(function() {
      assertEquals(settings.Route.BASIC, settings.getCurrentRoute());
      assertTrue(settings.lastRouteChangeWasPopstate());

      settings.navigateTo(settings.Route.ADVANCED);
      assertFalse(settings.lastRouteChangeWasPopstate());
    });
  });

  test('getRouteForPath trailing slashes', function() {
    assertEquals(settings.Route.BASIC, settings.getRouteForPath('/'));
    assertEquals(null, settings.getRouteForPath('//'));

    // Simple path.
    assertEquals(settings.Route.PEOPLE, settings.getRouteForPath('/people/'));
    assertEquals(settings.Route.PEOPLE, settings.getRouteForPath('/people'));

    // Path with a slash.
    assertEquals(
        settings.Route.SITE_SETTINGS_COOKIES,
        settings.getRouteForPath('/content/cookies'));
    assertEquals(
        settings.Route.SITE_SETTINGS_COOKIES,
        settings.getRouteForPath('/content/cookies/'));

    if (cr.isChromeOS) {
      // Path with a dash.
      assertEquals(
          settings.Route.KEYBOARD,
          settings.getRouteForPath('/keyboard-overlay'));
      assertEquals(
          settings.Route.KEYBOARD,
          settings.getRouteForPath('/keyboard-overlay/'));
    }
  });
});
