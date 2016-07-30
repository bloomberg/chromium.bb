// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('route', function() {
  test('tree structure', function() {
    // Set up root page routes.
    var BASIC = new settings.Route('/');
    var ADVANCED = new settings.Route('/advanced');
    assertDeepEquals([], ADVANCED.subpage);

    // Test a section route.
    var PRIVACY = ADVANCED.createChild('/privacy');
    PRIVACY.section = 'privacy';
    assertEquals(ADVANCED, PRIVACY.parent);
    assertDeepEquals([], PRIVACY.subpage);
    assertFalse(BASIC.contains(PRIVACY));
    assertTrue(ADVANCED.contains(PRIVACY));
    assertTrue(PRIVACY.contains(PRIVACY));
    assertFalse(PRIVACY.contains(ADVANCED));

    // Test a subpage route.
    var SITE_SETTINGS = PRIVACY.createChild('/siteSettings', 'site-settings');
    assertEquals('/siteSettings', SITE_SETTINGS.path);
    assertEquals(PRIVACY, SITE_SETTINGS.parent);
    assertFalse(!!SITE_SETTINGS.dialog);
    assertDeepEquals(['site-settings'], SITE_SETTINGS.subpage);
    assertEquals('privacy', SITE_SETTINGS.section);
    assertFalse(BASIC.contains(SITE_SETTINGS));
    assertTrue(ADVANCED.contains(SITE_SETTINGS));
    assertTrue(PRIVACY.contains(SITE_SETTINGS));

    // Test a sub-subpage route.
    var SITE_SETTINGS_ALL =
        SITE_SETTINGS.createChild('all', 'all-sites');
    assertEquals('/siteSettings/all', SITE_SETTINGS_ALL.path);
    assertEquals(SITE_SETTINGS, SITE_SETTINGS_ALL.parent);
    assertDeepEquals(['site-settings', 'all-sites'], SITE_SETTINGS_ALL.subpage);

    // Test a dialog route.
    var CLEAR_BROWSING_DATA =
        PRIVACY.createDialog('/clearBrowsingData', 'clear-browsing-data');
    assertEquals(PRIVACY, CLEAR_BROWSING_DATA.parent);
    assertEquals('clear-browsing-data', CLEAR_BROWSING_DATA.dialog);
    assertEquals('privacy', CLEAR_BROWSING_DATA.section);
    assertFalse(BASIC.contains(CLEAR_BROWSING_DATA));
    assertTrue(ADVANCED.contains(CLEAR_BROWSING_DATA));
    assertTrue(PRIVACY.contains(CLEAR_BROWSING_DATA));
  });

  test('no duplicate routes', function() {
    var paths = new Set();
    Object.values(settings.Route).forEach(function(route) {
      assertFalse(paths.has(route.path), route.path);
      paths.add(route.path);
    });
  });
});
