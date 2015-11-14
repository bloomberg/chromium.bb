// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
cr.define('extension_manager_tests', function() {
  /** @enum {string} */
  var TestNames = {
    SplitSections: 'split sections',
  };

  function registerTests() {
    suite('ExtensionManagerTest', function() {
      /** @type {extensions.Manager} */
      var manager;

      setup(function() {
        manager = document.querySelector('extensions-manager');
      });

      test(assert(TestNames.SplitSections), function() {
        var testVisible = extension_test_util.testVisible.bind(null, manager);
        // All sections and headers should be visible.
        testVisible('#extensions-header', true);
        testVisible('#extensions-list', true);
        testVisible('#apps-header', true);
        testVisible('#apps-list', true);
        testVisible('#websites-header', true);
        testVisible('#websites-list', true);

        var findItemWithName = function(name) {
          var result;
          manager.forEachItem(function(item) {
            if (item.data.name == name) {
              expectFalse(!!result,
                          'Found two items with the same name: ' + name);
              result = item;
            }
          });
          expectTrue(!!result);
          return result;
        };

        // Find each type of extension. Each should be present, visible, and
        // sorted in the correct section.
        var extension = findItemWithName('My extension 1');
        var selector = '#' + extension.data.id;
        testVisible(selector, true)
        expectTrue(!!manager.$$('#extensions-list').querySelector(selector));

        var platform_app =
            findItemWithName('Platform App Test: minimal platform app');
        selector = '#' + platform_app.data.id
        testVisible(selector, true)
        expectTrue(!!manager.$$('#apps-list').querySelector(selector));

        var hosted_app = findItemWithName('hosted_app');
        selector = '#' + hosted_app.data.id;
        testVisible(selector, true)
        expectTrue(!!manager.$$('#websites-list').querySelector(selector));

        var packaged_app = findItemWithName('Packaged App Test');
        selector = '#' + packaged_app.data.id;
        testVisible(selector, true)
        expectTrue(!!manager.$$('#websites-list').querySelector(selector));
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
