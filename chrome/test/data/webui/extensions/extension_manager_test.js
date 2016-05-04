// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
cr.define('extension_manager_tests', function() {
  /** @enum {string} */
  var TestNames = {
    ItemOrder: 'item order',
    ItemListVisibility: 'item list visibility',
    ShowItems: 'show items',
  };

  function getDataByName(list, name) {
    return assert(list.find(function(el) { return el.name == name; }));
  }

  function registerTests() {
    suite('ExtensionManagerTest', function() {
      /** @type {extensions.Manager} */
      var manager;

      setup(function() {
        manager = document.querySelector('extensions-manager');
      });

      test(assert(TestNames.ItemOrder), function() {
        expectEquals(0, manager.extensions.length);

        var alphaFromStore = extension_test_util.createExtensionInfo(
            {location: 'FROM_STORE', name: 'Alpha', id: 'a'.repeat(32)});
        manager.addItem(alphaFromStore);

        expectEquals(1, manager.extensions.length);
        expectEquals(alphaFromStore.id, manager.extensions[0].id);

        // Unpacked extensions come first.
        var betaUnpacked = extension_test_util.createExtensionInfo(
            {location: 'UNPACKED', name: 'Beta', id: 'b'.repeat(32)});
        manager.addItem(betaUnpacked);

        expectEquals(2, manager.extensions.length);
        expectEquals(betaUnpacked.id, manager.extensions[0].id);
        expectEquals(alphaFromStore.id, manager.extensions[1].id);

        // Extensions from the same location are sorted by name.
        var gammaUnpacked = extension_test_util.createExtensionInfo(
            {location: 'UNPACKED', name: 'Gamma', id: 'c'.repeat(32)});
        manager.addItem(gammaUnpacked);

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
        manager.addItem(aaFromStore);
        manager.addItem(AaFromStore);
        manager.addItem(aAFromStore);

        expectEquals(6, manager.extensions.length);
        expectEquals(betaUnpacked.id, manager.extensions[0].id);
        expectEquals(gammaUnpacked.id, manager.extensions[1].id);
        expectEquals(aaFromStore.id, manager.extensions[2].id);
        expectEquals(AaFromStore.id, manager.extensions[3].id);
        expectEquals(aAFromStore.id, manager.extensions[4].id);
        expectEquals(alphaFromStore.id, manager.extensions[5].id);
      });

      test(assert(TestNames.ItemListVisibility), function() {
        var testVisible = extension_test_util.testVisible.bind(null, manager);
        var extension = getDataByName(manager.extensions, 'My extension 1');

        var listHasItemWithName = function(name) {
          return !!manager.$['items-list'].items.find(function(el) {
            return el.name == name;
          });
        };

        expectEquals(manager.extensions, manager.$['items-list'].items);
        testVisible('#items-list', true);
        expectTrue(listHasItemWithName('My extension 1'));

        manager.removeItem(extension);
        Polymer.dom.flush();
        testVisible('#items-list', false);
        expectFalse(listHasItemWithName('My extension 1'));

        manager.addItem(extension);
        Polymer.dom.flush();
        testVisible('#items-list', true);
        expectTrue(listHasItemWithName('My extension 1'));
      });

      test(assert(TestNames.ShowItems), function() {
        var sectionHasItemWithName = function(section, name) {
          return !!manager[section].find(function(el) {
            return el.name == name;
          });
        };

        // Test that we properly split up the items into two sections.
        expectTrue(sectionHasItemWithName('extensions', 'My extension 1'));
        expectTrue(sectionHasItemWithName(
            'apps', 'Platform App Test: minimal platform app'));
        expectTrue(sectionHasItemWithName('apps', 'hosted_app'));
        expectTrue(sectionHasItemWithName('apps', 'Packaged App Test'));

        // Toggle between extensions and apps and back again.
        expectEquals(manager.extensions, manager.$['items-list'].items);
        manager.listHelper_.showType(extensions.ShowingType.APPS);
        expectEquals(manager.apps, manager.$['items-list'].items);
        manager.listHelper_.showType(extensions.ShowingType.EXTENSIONS);
        expectEquals(manager.extensions, manager.$['items-list'].items);
        // Repeating a selection should have no change.
        manager.listHelper_.showType(extensions.ShowingType.EXTENSIONS);
        expectEquals(manager.extensions, manager.$['items-list'].items);
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
