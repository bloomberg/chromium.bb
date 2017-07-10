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

  teardown(function() {
    PolymerTest.clearBody();
  });

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
    Object.values(settings.routes).forEach(function(route) {
      assertFalse(paths.has(route.path), route.path);
      paths.add(route.path);
    });
  });

  test('navigate back to parent previous route', function() {
    return testNavigateBackUsesHistory(
        settings.routes.BASIC, settings.routes.PEOPLE, settings.routes.BASIC);
  });

  test('navigate back to non-ancestor shallower route', function() {
    return testNavigateBackUsesHistory(
        settings.routes.ADVANCED, settings.routes.PEOPLE,
        settings.routes.BASIC);
  });

  test('navigate back to sibling route', function() {
    return testNavigateBackUsesHistory(
        settings.routes.APPEARANCE, settings.routes.PEOPLE,
        settings.routes.APPEARANCE);
  });

  test('navigate back to parent when previous route is deeper', function() {
    settings.navigateTo(settings.routes.SYNC);
    settings.navigateTo(settings.routes.PEOPLE);
    settings.navigateToPreviousRoute();
    assertEquals(settings.routes.BASIC, settings.getCurrentRoute());
  });

  test('navigate back to BASIC when going back from root pages', function() {
    settings.navigateTo(settings.routes.PEOPLE);
    settings.navigateTo(settings.routes.ADVANCED);
    settings.navigateToPreviousRoute();
    assertEquals(settings.routes.BASIC, settings.getCurrentRoute());
  });

  test('navigateTo respects removeSearch optional parameter', function() {
    var params = new URLSearchParams('search=foo');
    settings.navigateTo(settings.routes.BASIC, params);
    assertEquals(params.toString(), settings.getQueryParameters().toString());

    settings.navigateTo(
        settings.routes.SITE_SETTINGS, null,
        /* removeSearch */ false);
    assertEquals(params.toString(), settings.getQueryParameters().toString());

    settings.navigateTo(
        settings.routes.SEARCH_ENGINES, null,
        /* removeSearch */ true);
    assertEquals('', settings.getQueryParameters().toString());
  });

  test('navigateTo ADVANCED forwards to BASIC', function() {
    settings.navigateTo(settings.routes.ADVANCED);
    assertEquals(settings.routes.BASIC, settings.getCurrentRoute());
  });

  test('popstate flag works', function() {
    settings.navigateTo(settings.routes.BASIC);
    assertFalse(settings.lastRouteChangeWasPopstate());

    settings.navigateTo(settings.routes.PEOPLE);
    assertFalse(settings.lastRouteChangeWasPopstate());

    return whenPopState(function() {
             window.history.back();
           })
        .then(function() {
          assertEquals(settings.routes.BASIC, settings.getCurrentRoute());
          assertTrue(settings.lastRouteChangeWasPopstate());

          settings.navigateTo(settings.routes.ADVANCED);
          assertFalse(settings.lastRouteChangeWasPopstate());
        });
  });

  test('getRouteForPath trailing slashes', function() {
    assertEquals(settings.routes.BASIC, settings.getRouteForPath('/'));
    assertEquals(null, settings.getRouteForPath('//'));

    // Simple path.
    assertEquals(settings.routes.PEOPLE, settings.getRouteForPath('/people/'));
    assertEquals(settings.routes.PEOPLE, settings.getRouteForPath('/people'));

    // Path with a slash.
    assertEquals(
        settings.routes.SITE_SETTINGS_COOKIES,
        settings.getRouteForPath('/content/cookies'));
    assertEquals(
        settings.routes.SITE_SETTINGS_COOKIES,
        settings.getRouteForPath('/content/cookies/'));

    if (cr.isChromeOS) {
      // Path with a dash.
      assertEquals(
          settings.routes.KEYBOARD,
          settings.getRouteForPath('/keyboard-overlay'));
      assertEquals(
          settings.routes.KEYBOARD,
          settings.getRouteForPath('/keyboard-overlay/'));
    }
  });

  test('isNavigableDialog', function() {
    assertTrue(settings.routes.CLEAR_BROWSER_DATA.isNavigableDialog);
    assertTrue(settings.routes.IMPORT_DATA.isNavigableDialog);
    assertTrue(settings.routes.RESET_DIALOG.isNavigableDialog);
    assertTrue(settings.routes.SIGN_OUT.isNavigableDialog);
    assertTrue(settings.routes.TRIGGERED_RESET_DIALOG.isNavigableDialog);

    assertFalse(settings.routes.PRIVACY.isNavigableDialog);
    assertFalse(settings.routes.DEFAULT_BROWSER.isNavigableDialog);
  });

  test('pageVisibility affects route availability', function() {
    settings.pageVisibility = {
      advancedSettings: false,
      appearance: false,
      defaultBrowser: false,
      onStartup: false,
      passwordsAndForms: false,
      people: false,
      reset: false,
    };

    var router = new settings.Router();
    var hasRoute = route => router.getRoutes().hasOwnProperty(route);

    assertTrue(hasRoute('BASIC'));

    assertFalse(hasRoute('ADVANCED'));
    assertFalse(hasRoute('APPEARANCE'));
    assertFalse(hasRoute('DEFAULT_BROWSER'));
    assertFalse(hasRoute('ON_STARTUP'));
    assertFalse(hasRoute('PASSWORDS'));
    assertFalse(hasRoute('PEOPLE'));
    assertFalse(hasRoute('RESET'));
  });
});
