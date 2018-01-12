// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-toolbar. */
cr.define('extension_toolbar_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Layout: 'layout',
    ClickHandlers: 'click handlers',
    DevModeToggle: 'dev mode toggle',
  };

  var suiteName = 'ExtensionToolbarTest';
  suite(suiteName, function() {
    /** @type {MockDelegate} */
    var mockDelegate;

    /** @type {extensions.Toolbar} */
    var toolbar;

    setup(function() {
      toolbar = document.querySelector('extensions-manager').$$(
          'extensions-toolbar');
      mockDelegate = new extensions.TestService();
      toolbar.set('delegate', mockDelegate);
    });

    test(assert(TestNames.Layout), function() {
      extension_test_util.testIcons(toolbar);

      var testVisible = extension_test_util.testVisible.bind(null, toolbar);
      testVisible('#dev-mode', true);
      assertEquals(toolbar.$$('#dev-mode').disabled, false);
      testVisible('#load-unpacked', false);
      testVisible('#pack-extensions', false);
      testVisible('#update-now', false);

      toolbar.set('inDevMode', true);
      Polymer.dom.flush();

      testVisible('#dev-mode', true);
      assertEquals(toolbar.$$('#dev-mode').disabled, false);
      testVisible('#load-unpacked', true);
      testVisible('#pack-extensions', true);
      testVisible('#update-now', true);

      toolbar.set('canLoadUnpacked', false);
      Polymer.dom.flush();

      testVisible('#dev-mode', true);
      testVisible('#load-unpacked', false);
      testVisible('#pack-extensions', true);
      testVisible('#update-now', true);
    });

    test(assert(TestNames.DevModeToggle), function() {
      const toggle = toolbar.$$('#dev-mode');
      assertFalse(toggle.disabled);

      // Test that the dev-mode toggle is disabled when a policy exists.
      toolbar.set('devModeControlledByPolicy', true);
      Polymer.dom.flush();
      assertTrue(toggle.disabled);

      toolbar.set('devModeControlledByPolicy', false);
      Polymer.dom.flush();
      assertFalse(toggle.disabled);

      // Test that the dev-mode toggle is disabled when the user is supervised.
      toolbar.set('isSupervised', true);
      Polymer.dom.flush();
      assertTrue(toggle.disabled);
    });

    test(assert(TestNames.ClickHandlers), function() {
      toolbar.set('inDevMode', true);
      Polymer.dom.flush();

      MockInteractions.tap(toolbar.$['dev-mode']);
      return mockDelegate.whenCalled('setProfileInDevMode').then(function(arg) {
        assertFalse(arg);
        mockDelegate.reset();
        MockInteractions.tap(toolbar.$['dev-mode']);
        return mockDelegate.whenCalled('setProfileInDevMode');
      }).then(function(arg) {
        assertTrue(arg);
        MockInteractions.tap(toolbar.$$('#load-unpacked'));
        return mockDelegate.whenCalled('loadUnpacked');
      }).then(function() {
        MockInteractions.tap(toolbar.$$('#update-now'));
        return mockDelegate.whenCalled('updateAllExtensions');
      }).then(function() {
        var listener = new extension_test_util.ListenerMock();
        listener.addListener(toolbar, 'pack-tap');
        MockInteractions.tap(toolbar.$$('#pack-extensions'));
        listener.verify();
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
