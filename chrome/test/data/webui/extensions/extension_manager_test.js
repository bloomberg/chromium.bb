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
        var testManagerElementVisible =
            extension_test_util.testVisible.bind(null, manager);
        // All sections should be visible.
        testManagerElementVisible('#extensions-list', true);
        testManagerElementVisible('#apps-list', true);
        testManagerElementVisible('#websites-list', true);

        var sectionHasItemWithName = function(section, name) {
          return !!manager[section].find(function(el) {
            return el.name == name;
          });
        };

        expectEquals(manager.extensions, manager.$['extensions-list'].items);
        expectEquals(manager.apps, manager.$['apps-list'].items);
        expectEquals(manager.websites, manager.$['websites-list'].items);

        // We really just have to test for existence of the items within the
        // given subsection of the manager, since they are bound to the iron
        // list with Polymer (and we kind of have to trust that Polymer works).
        expectTrue(sectionHasItemWithName('extensions', 'My extension 1'));
        expectTrue(sectionHasItemWithName(
            'apps', 'Platform App Test: minimal platform app'));
        expectTrue(sectionHasItemWithName('websites', 'hosted_app'));
        expectTrue(sectionHasItemWithName('websites', 'Packaged App Test'));
      });

      test(assert(TestNames.ItemOrder), function() {
        var service = extensions.Service.getInstance();
        expectEquals(0, manager.extensions.length);

        var alphaFromStore = extension_test_util.createExtensionInfo(
            {location: 'FROM_STORE', name: 'Alpha', id: 'a'.repeat(32)});
        manager.addItem(alphaFromStore, service);

        expectEquals(1, manager.extensions.length);
        expectEquals(alphaFromStore.id, manager.extensions[0].id);

        // Unpacked extensions come first.
        var betaUnpacked = extension_test_util.createExtensionInfo(
            {location: 'UNPACKED', name: 'Beta', id: 'b'.repeat(32)});
        manager.addItem(betaUnpacked, service);

        expectEquals(2, manager.extensions.length);
        expectEquals(betaUnpacked.id, manager.extensions[0].id);
        expectEquals(alphaFromStore.id, manager.extensions[1].id);

        // Extensions from the same location are sorted by name.
        var gammaUnpacked = extension_test_util.createExtensionInfo(
            {location: 'UNPACKED', name: 'Gamma', id: 'c'.repeat(32)});
        manager.addItem(gammaUnpacked, service);

        expectEquals(3, manager.extensions.length);
        expectEquals(betaUnpacked.id, manager.extensions[0].id);
        expectEquals(gammaUnpacked.id, manager.extensions[1].id);
        expectEquals(alphaFromStore.id, manager.extensions[2].id);

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

        expectEquals(6, manager.extensions.length);
        expectEquals(betaUnpacked.id, manager.extensions[0].id);
        expectEquals(gammaUnpacked.id, manager.extensions[1].id);
        expectEquals(aaFromStore.id, manager.extensions[2].id);
        expectEquals(AaFromStore.id, manager.extensions[3].id);
        expectEquals(aAFromStore.id, manager.extensions[4].id);
        expectEquals(alphaFromStore.id, manager.extensions[5].id);
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
