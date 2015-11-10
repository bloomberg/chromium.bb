// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
cr.define('extension_sidebar_tests', function() {
  /** @type {extensions.Sidebar} */
  var sidebar;

  /**
   * A mock delegate for the sidebar, capable of testing functionality.
   * @constructor
   * @implements {extensions.SidebarDelegate}
   */
  function MockDelegate() {}

  MockDelegate.prototype = {
    /**
     * Tests clicking on an element and expected a delegate call from the
     * sidebar.
     * @param {HTMLElement} element The element to click on.
     * @param {string} callName The function expected to be called.
     * @param {Array<?>=} opt_expectedArgs The arguments the function is
     *     expected to be called with.
     */
    testClickingCalls: function(element, callName, opt_expectedArgs) {
      var mock = new MockController();
      var mockMethod = mock.createFunctionMock(this, callName);
      MockMethod.prototype.addExpectation.apply(
          mockMethod, opt_expectedArgs);
      MockInteractions.tap(element);
      mock.verifyMocks();
    },

    /** @override */
    setProfileInDevMode: function(inDevMode) {},

    /** @override */
    loadUnpacked: function() {},

    /** @override */
    packExtension: function() {},

    /** @override */
    updateAllExtensions: function() {},
  };

  /**
   * Tests that the element's visibility matches |expectedVisible|.
   * @param {boolean} expectedVisible Whether the element should be
   *     visible.
   * @param {string} selector The selector to find the element.
   */
  function testVisible(selector, expectedVisible) {
    var element = sidebar.$$(selector);
    var rect = element ? element.getBoundingClientRect() : null;
    var isVisible = !!rect && (rect.width * rect.height > 0);
    expectEquals(expectedVisible, isVisible, selector);
  }

  function registerTests() {
    suite('ExtensionSidebarTest', function() {
      /** @type {MockDelegate} */
      var mockDelegate;

      // Import cr_settings_checkbox.html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml('chrome://extensions/sidebar.html');
      });

      setup(function() {
        sidebar = document.querySelector('extensions-manager').sidebar;
        mockDelegate = new MockDelegate();
        sidebar.setDelegate(mockDelegate);
      });

      test('test sidebar layout', function() {
        testVisible('#load-unpacked', false);
        testVisible('#pack-extensions', false);
        testVisible('#update-now', false);

        sidebar.set('inDevMode', true);
        Polymer.dom.flush();

        testVisible('#load-unpacked', true);
        testVisible('#pack-extensions', true);
        testVisible('#update-now', true);
      });

      test('test sidebar click handlers', function() {
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
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
