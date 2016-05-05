// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
cr.define('extension_sidebar_tests', function() {
  /**
   * A mock delegate for the sidebar.
   * @constructor
   * @implements {extensions.SidebarDelegate}
   * @implements {extensions.SidebarScrollDelegate}
   * @extends {extension_test_util.ClickMock}
   */
  function MockDelegate() {}

  MockDelegate.prototype = {
    __proto__: extension_test_util.ClickMock.prototype,

    /** @override */
    setProfileInDevMode: function(inDevMode) {},

    /** @override */
    loadUnpacked: function() {},

    /** @override */
    packExtension: function() {},

    /** @override */
    updateAllExtensions: function() {},

    /** @override */
    showType: function() {},
  };

  /** @enum {string} */
  var TestNames = {
    Layout: 'layout',
    ClickHandlers: 'click handlers',
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
        sidebar = document.querySelector('extensions-manager').sidebar;
        mockDelegate = new MockDelegate();
        sidebar.setDelegate(mockDelegate);
        sidebar.setListDelegate(mockDelegate);
      });

      test(assert(TestNames.Layout), function() {
        extension_test_util.testIronIcons(sidebar);

        var testVisible = extension_test_util.testVisible.bind(null, sidebar);
        testVisible('#load-unpacked', false);
        testVisible('#pack-extensions', false);
        testVisible('#update-now', false);

        sidebar.set('inDevMode', true);
        Polymer.dom.flush();

        testVisible('#load-unpacked', true);
        testVisible('#pack-extensions', true);
        testVisible('#update-now', true);
      });

      test(assert(TestNames.ClickHandlers), function() {
        sidebar.set('inDevMode', true);
        Polymer.dom.flush();

        mockDelegate.testClickingCalls(
            sidebar.$['developer-mode-checkbox'], 'setProfileInDevMode',
            [false]);
        mockDelegate.testClickingCalls(
            sidebar.$['developer-mode-checkbox'], 'setProfileInDevMode',
            [true]);
        mockDelegate.testClickingCalls(
            sidebar.$$('#load-unpacked'), 'loadUnpacked', []);
        mockDelegate.testClickingCalls(
            sidebar.$$('#pack-extensions'), 'packExtension', []);
        mockDelegate.testClickingCalls(
            sidebar.$$('#update-now'), 'updateAllExtensions', []);
        mockDelegate.testClickingCalls(
            sidebar.$$('#sections-extensions'), 'showType',
            [extensions.ShowingType.EXTENSIONS]);
        mockDelegate.testClickingCalls(
            sidebar.$$('#sections-apps'), 'showType',
            [extensions.ShowingType.APPS]);
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
