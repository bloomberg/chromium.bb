// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-toolbar. */
cr.define('extension_toolbar_tests', function() {
  /**
   * A mock delegate for the toolbar.
   * @constructor
   * @implements {extensions.ToolbarDelegate}
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
    updateAllExtensions: function() {},
  };

  /** @enum {string} */
  var TestNames = {
    Layout: 'layout',
    ClickHandlers: 'click handlers',
  };

  function registerTests() {
    suite('ExtensionToolbarTest', function() {
      /** @type {MockDelegate} */
      var mockDelegate;

      /** @type {extensions.Toolbar} */
      var toolbar;

      setup(function() {
        toolbar = document.querySelector('extensions-manager').toolbar;
        mockDelegate = new MockDelegate();
        toolbar.set('delegate', mockDelegate);
      });

      test(assert(TestNames.Layout), function() {
        extension_test_util.testIronIcons(toolbar);

        var testVisible = extension_test_util.testVisible.bind(null, toolbar);
        testVisible('#dev-mode', true);
        testVisible('#load-unpacked', false);
        testVisible('#pack-extensions', false);
        testVisible('#update-now', false);

        toolbar.set('inDevMode', true);
        Polymer.dom.flush();

        testVisible('#dev-mode', true);
        testVisible('#load-unpacked', true);
        testVisible('#pack-extensions', true);
        testVisible('#update-now', true);
      });

      test(assert(TestNames.ClickHandlers), function() {
        toolbar.set('inDevMode', true);
        Polymer.dom.flush();

        mockDelegate.testClickingCalls(
            toolbar.$['dev-mode'], 'setProfileInDevMode',
            [false]);
        mockDelegate.testClickingCalls(
            toolbar.$['dev-mode'], 'setProfileInDevMode',
            [true]);
        mockDelegate.testClickingCalls(
            toolbar.$$('#load-unpacked'), 'loadUnpacked', []);
        mockDelegate.testClickingCalls(
            toolbar.$$('#update-now'), 'updateAllExtensions', []);

        var listener = new extension_test_util.ListenerMock();
        listener.addListener(toolbar, 'pack-tap');
        MockInteractions.tap(toolbar.$$('#pack-extensions'));
        listener.verify();
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
