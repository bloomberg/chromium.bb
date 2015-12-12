// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
cr.define('extension_manager_tests', function() {
  /** @enum {string} */
  var TestNames = {
    SplitSections: 'split sections',
    ItemOrder: 'item order',
    ExtensionSectionVisibility: 'extension section visibility',
    AppSectionVisibility: 'app section visibility',
    WebsiteSectionVisibility: 'website section visibility',
    Scrolling: 'scrolling',
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

      test(assert(TestNames.ExtensionSectionVisibility), function() {
        var testVisible = extension_test_util.testVisible.bind(null, manager);
        var testSidebarVisible =
             extension_test_util.testVisible.bind(null, manager.sidebar);

        var extension = getDataByName(manager.extensions, 'My extension 1');

        testVisible('#extensions-list', true);
        testSidebarVisible('#sections-extensions', true);
        manager.removeItem(extension);
        Polymer.dom.flush();
        testVisible('#extensions-list', false);
        testSidebarVisible('#sections-extensions', false);

        manager.addItem(extension);
        Polymer.dom.flush();
        testVisible('#extensions-list', true);
        testSidebarVisible('#sections-extensions', true);
      });

      test(assert(TestNames.AppSectionVisibility), function() {
        var testVisible = extension_test_util.testVisible.bind(null, manager);
        var testSidebarVisible =
             extension_test_util.testVisible.bind(null, manager.sidebar);
        var platformApp =
            getDataByName(manager.apps,
                          'Platform App Test: minimal platform app');

        testVisible('#apps-list', true);
        testSidebarVisible('#sections-apps', true);
        manager.removeItem(platformApp);
        Polymer.dom.flush();
        testVisible('#apps-list', false);
        testSidebarVisible('#sections-apps', false);

        manager.addItem(platformApp);
        Polymer.dom.flush();
        testVisible('#apps-list', true);
        testSidebarVisible('#sections-apps', true);
      });

      test(assert(TestNames.WebsiteSectionVisibility), function() {
        var testVisible = extension_test_util.testVisible.bind(null, manager);
        var testSidebarVisible =
             extension_test_util.testVisible.bind(null, manager.sidebar);

        var hostedApp = getDataByName(manager.websites, 'hosted_app');
        var packagedApp = getDataByName(manager.websites, 'Packaged App Test');

        testVisible('#websites-list', true);
        testSidebarVisible('#sections-websites', true);
        expectEquals(2, manager.websites.length);
        manager.removeItem(hostedApp);
        Polymer.dom.flush();
        expectEquals(1, manager.websites.length);
        testVisible('#websites-list', true);
        testSidebarVisible('#sections-websites', true);
        manager.removeItem(packagedApp);
        Polymer.dom.flush();
        testVisible('#websites-list', false);
        testSidebarVisible('#sections-websites', false);

        manager.addItem(hostedApp);
        Polymer.dom.flush();
        testVisible('#websites-list', true);
        testSidebarVisible('#sections-websites', true);
      });

      test(assert(TestNames.Scrolling), function() {
        // TODO(devlin): This doesn't really test anything, because scrolling is
        // so heavily dependent on viewport size (which is very unpredictable in
        // browser tests). But we can at least exercise the code path and check
        // for errors.
        manager.scrollHelper_.scrollToExtensions();
        manager.scrollHelper_.scrollToApps();
        manager.scrollHelper_.scrollToWebsites();
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
