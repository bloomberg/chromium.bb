// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
cr.define('extension_manager_tests', function() {
  /** @enum {string} */
  var TestNames = {
    SplitSections: 'split sections',
    ItemOrder: 'item order',
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

      test(assert(TestNames.ItemOrder), function() {
        var extensionsSection = manager.$['extensions-list'];
        var service = extensions.Service.getInstance();
        expectEquals(0, extensionsSection.children.length);

        var alphaFromStore = extension_test_util.createExtensionInfo(
            {location: 'FROM_STORE', name: 'Alpha', id: 'a'.repeat(32)});
        manager.addItem(alphaFromStore, service);

        expectEquals(1, extensionsSection.children.length);
        expectEquals(alphaFromStore.id, extensionsSection.children[0].id);

        // Unpacked extensions come first.
        var betaUnpacked = extension_test_util.createExtensionInfo(
            {location: 'UNPACKED', name: 'Beta', id: 'b'.repeat(32)});
        manager.addItem(betaUnpacked, service);

        expectEquals(2, extensionsSection.children.length);
        expectEquals(betaUnpacked.id, extensionsSection.children[0].id);
        expectEquals(alphaFromStore.id, extensionsSection.children[1].id);

        // Extensions from the same location are sorted by name.
        var gammaUnpacked = extension_test_util.createExtensionInfo(
            {location: 'UNPACKED', name: 'Gamma', id: 'c'.repeat(32)});
        manager.addItem(gammaUnpacked, service);

        expectEquals(3, extensionsSection.children.length);
        expectEquals(betaUnpacked.id, extensionsSection.children[0].id);
        expectEquals(gammaUnpacked.id, extensionsSection.children[1].id);
        expectEquals(alphaFromStore.id, extensionsSection.children[2].id);

        // The name-sort should be case-insensitive, and should fall back on
        // id.
        var aaFromStore = extension_test_util.createExtensionInfo(
            {location: 'FROM_STORE', name: 'AA', id: 'd'.repeat(32)});
        var AaFromStore = extension_test_util.createExtensionInfo(
            {location: 'FROM_STORE', name: 'Aa', id: 'e'.repeat(32)});
        var aAFromStore = extension_test_util.createExtensionInfo(
            {location: 'FROM_STORE', name: 'aA', id: 'f'.repeat(32)});
        manager.addItem(aaFromStore, service);
        manager.addItem(AaFromStore, service);
        manager.addItem(aAFromStore, service);

        expectEquals(6, extensionsSection.children.length);
        expectEquals(betaUnpacked.id, extensionsSection.children[0].id);
        expectEquals(gammaUnpacked.id, extensionsSection.children[1].id);
        expectEquals(aaFromStore.id, extensionsSection.children[2].id);
        expectEquals(AaFromStore.id, extensionsSection.children[3].id);
        expectEquals(aAFromStore.id, extensionsSection.children[4].id);
        expectEquals(alphaFromStore.id, extensionsSection.children[5].id);
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
