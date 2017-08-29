// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
cr.define('extension_sidebar_tests', function() {
  /** @enum {string} */
  var TestNames = {
    LayoutAndClickHandlers: 'layout and click handlers',
  };

  suite('ExtensionSidebarTest', function() {
    /** @type {extensions.Sidebar} */
    var sidebar;

    // Import cr_settings_checkbox.html before running suite.
    suiteSetup(function() {
      return PolymerTest.importHtml('chrome://extensions/sidebar.html');
    });

    setup(function() {
      var manager = document.querySelector('extensions-manager');
      manager.$.drawer.openDrawer();
      sidebar = manager.$.sidebar;
    });

    test(assert(TestNames.LayoutAndClickHandlers), function() {
      extension_test_util.testIronIcons(sidebar);

      var testVisible = extension_test_util.testVisible.bind(null, sidebar);
      testVisible('#sections-extensions', true);
      testVisible('#sections-apps', true);
      testVisible('#sections-shortcuts', true);
      testVisible('#more-extensions', true);

      var currentPage;
      extensions.navigation.onRouteChanged(newPage => {
        currentPage = newPage;
      });

      MockInteractions.tap(sidebar.$$('#sections-apps'));
      expectDeepEquals(
          currentPage, {page: Page.LIST, type: extensions.ShowingType.APPS});

      MockInteractions.tap(sidebar.$$('#sections-extensions'));
      expectDeepEquals(
          currentPage,
          {page: Page.LIST, type: extensions.ShowingType.EXTENSIONS});

      MockInteractions.tap(sidebar.$$('#sections-shortcuts'));
      expectDeepEquals(currentPage, {page: Page.SHORTCUTS});
    });
  });

  return {
    TestNames: TestNames,
  };
});
