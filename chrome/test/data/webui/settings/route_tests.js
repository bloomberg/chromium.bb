// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('route', function() {
  test('tree structure', function() {
    // Set up root page routes.
    var BASIC = new settings.Route('/');
    BASIC.page = 'basic';
    var ADVANCED = new settings.Route('/advanced');
    ADVANCED.page = 'advanced';
    assertDeepEquals([], ADVANCED.subpage);

    // Test a section route.
    var PRIVACY = ADVANCED.createChild('/privacy');
    PRIVACY.section = 'privacy';
    assertEquals('advanced', PRIVACY.page);
    assertDeepEquals([], PRIVACY.subpage);
    assertFalse(PRIVACY.isDescendantOf(BASIC));
    assertTrue(PRIVACY.isDescendantOf(ADVANCED));
    assertFalse(PRIVACY.isDescendantOf(PRIVACY));
    assertFalse(ADVANCED.isDescendantOf(PRIVACY));

    // Test a subpage route.
    var SITE_SETTINGS = PRIVACY.createChild('/siteSettings', 'site-settings');
    assertEquals('/siteSettings', SITE_SETTINGS.url);
    assertFalse(!!SITE_SETTINGS.dialog);
    assertDeepEquals(['site-settings'], SITE_SETTINGS.subpage);
    assertEquals('advanced', SITE_SETTINGS.page);
    assertEquals('privacy', SITE_SETTINGS.section);
    assertFalse(SITE_SETTINGS.isDescendantOf(BASIC));
    assertTrue(SITE_SETTINGS.isDescendantOf(ADVANCED));
    assertTrue(SITE_SETTINGS.isDescendantOf(PRIVACY));

    // Test a sub-subpage route.
    var SITE_SETTINGS_ALL =
        SITE_SETTINGS.createChild('all', 'all-sites');
    assertEquals('/siteSettings/all', SITE_SETTINGS_ALL.url);
    assertDeepEquals(['site-settings', 'all-sites'], SITE_SETTINGS_ALL.subpage);

    // Test a dialog route.
    var CLEAR_BROWSING_DATA =
        PRIVACY.createDialog('/clearBrowsingData', 'clear-browsing-data');
    assertEquals('clear-browsing-data', CLEAR_BROWSING_DATA.dialog);
    assertEquals('privacy', CLEAR_BROWSING_DATA.section);
    assertEquals('advanced', CLEAR_BROWSING_DATA.page);
    assertEquals('privacy', CLEAR_BROWSING_DATA.section);
    assertFalse(CLEAR_BROWSING_DATA.isDescendantOf(BASIC));
    assertTrue(CLEAR_BROWSING_DATA.isDescendantOf(ADVANCED));
    assertTrue(CLEAR_BROWSING_DATA.isDescendantOf(PRIVACY));
  });

  test('no duplicate routes', function() {
    var urls = new Set();
    Object.values(settings.Route).forEach(function(route) {
      assertFalse(urls.has(route.url), route.url);
      urls.add(route.url);
    });
  });
});
