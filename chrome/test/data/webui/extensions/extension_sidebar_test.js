// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
cr.define('extension_sidebar_tests', function() {
  /**
   * A mock delegate for the sidebar.
   * @constructor
   * @implements {extensions.SidebarListDelegate}
   * @extends {extension_test_util.ClickMock}
   */
  function MockDelegate() {}

  MockDelegate.prototype = {
    __proto__: extension_test_util.ClickMock.prototype,

    /** @override */
    showType: function() {},

    /** @override */
    showKeyboardShortcuts: function() {},
  };

  /** @enum {string} */
  var TestNames = {
    LayoutAndClickHandlers: 'layout and click handlers',
  };

  function registerTests() {
    suite('ExtensionSidebarTest', function() {
      /** @type {MockDelegate} */
      var mockDelegate;

      /** @type {extensions.Sidebar} */
      var sidebar;

      // Import cr_settings_checkbox.html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml('chrome://extensions/sidebar.html');
      });

      setup(function() {
        var manager = document.querySelector('extensions-manager');
        manager.$.drawer.openDrawer();
        sidebar = manager.sidebar;
        mockDelegate = new MockDelegate();
        sidebar.setListDelegate(mockDelegate);
      });

      test(assert(TestNames.LayoutAndClickHandlers), function() {
        extension_test_util.testIronIcons(sidebar);

        var testVisible = extension_test_util.testVisible.bind(null, sidebar);
        testVisible('#sections-extensions', true);
        testVisible('#sections-apps', true);
        testVisible('#sections-shortcuts', true);
        testVisible('#more-extensions', true);

        mockDelegate.testClickingCalls(
            sidebar.$$('#sections-extensions'), 'showType',
            [extensions.ShowingType.EXTENSIONS]);
        mockDelegate.testClickingCalls(
            sidebar.$$('#sections-apps'), 'showType',
            [extensions.ShowingType.APPS]);
        mockDelegate.testClickingCalls(
            sidebar.$$('#sections-shortcuts'), 'showKeyboardShortcuts', []);
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
