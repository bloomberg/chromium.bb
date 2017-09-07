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
    ChangePages: 'change pages',
    UrlNavigationToDetails: 'url navigation to details',
    UpdateItemData: 'update item data',
    SidebarHighlighting: 'sidebar highlighting',
  };

  function getDataByName(list, name) {
    return assert(list.find(function(el) { return el.name == name; }));
  }

  suite('ExtensionManagerTest', function() {
    /** @type {extensions.Manager} */
    var manager;

    function isActiveView(viewId) {
      expectEquals(viewId, manager.$.viewManager.querySelector('.active').id);
    }

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
      var extension = getDataByName(manager.extensions, 'My extension 1');

      var listHasItemWithName = function(name) {
        return !!manager.$['items-list'].items.find(function(el) {
          return el.name == name;
        });
      };

      expectEquals(manager.extensions, manager.$['items-list'].items);
      expectTrue(listHasItemWithName('My extension 1'));

      manager.removeItem(extension);
      Polymer.dom.flush();
      expectFalse(listHasItemWithName('My extension 1'));

      manager.addItem(extension);
      Polymer.dom.flush();
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
      extensions.navigation.navigateTo(
          {page: Page.LIST, type: extensions.ShowingType.APPS});
      expectEquals(manager.apps, manager.$['items-list'].items);
      extensions.navigation.navigateTo(
          {page: Page.LIST, type: extensions.ShowingType.EXTENSIONS});
      expectEquals(manager.extensions, manager.$['items-list'].items);
      // Repeating a selection should have no change.
      extensions.navigation.navigateTo(
          {page: Page.LIST, type: extensions.ShowingType.EXTENSIONS});
      expectEquals(manager.extensions, manager.$['items-list'].items);
    });

    test(assert(TestNames.SidebarHighlighting), function() {
      const toolbar = manager.$$('extensions-toolbar');

      // Stub manager.$.sidebar for testing convenience.
      let pageState;
      manager.$.sidebar = {
        updateSelected: function(state) {
          pageState = state;
        },
      };

      // Tests if manager calls sidebar.updateSelected with the right params.
      function testPassingCorrectState(onPageState, passedPageState) {
        extensions.navigation.navigateTo(onPageState);
        toolbar.fire('cr-toolbar-menu-tap');
        expectDeepEquals(pageState, passedPageState || onPageState);
        // Reset pageState so that testing order doesn't matter.
        pageState = null;
      }

      testPassingCorrectState({page: Page.SHORTCUTS});
      testPassingCorrectState(
          {page: Page.LIST, type: extensions.ShowingType.APPS});
      testPassingCorrectState(
          {page: Page.LIST, type: extensions.ShowingType.EXTENSIONS});

      testPassingCorrectState(
          {page: Page.DETAILS, extensionId: manager.apps[0].id},
          {page: Page.LIST, type: extensions.ShowingType.APPS});
      testPassingCorrectState(
          {page: Page.DETAILS, extensionId: manager.extensions[0].id},
          {page: Page.LIST, type: extensions.ShowingType.EXTENSIONS});
      testPassingCorrectState(
          {page: Page.ERRORS, extensionId: manager.apps[0].id},
          {page: Page.LIST, type: extensions.ShowingType.APPS});
      testPassingCorrectState(
          {page: Page.ERRORS, extensionId: manager.extensions[0].id},
          {page: Page.LIST, type: extensions.ShowingType.EXTENSIONS});
    });

    test(assert(TestNames.ChangePages), function() {
      // We start on the item list.
      MockInteractions.tap(manager.$.sidebar.$['sections-extensions']);
      Polymer.dom.flush();
      isActiveView(Page.LIST);

      // Switch: item list -> keyboard shortcuts.
      MockInteractions.tap(manager.$.sidebar.$['sections-shortcuts']);
      Polymer.dom.flush();
      isActiveView(Page.SHORTCUTS);

      // Switch: keyboard shortcuts -> item list.
      MockInteractions.tap(manager.$.sidebar.$['sections-apps']);
      Polymer.dom.flush();
      isActiveView(Page.LIST);

      // Switch: item list -> detail view.
      var item = manager.$['items-list'].$$('extensions-item');
      assert(item);
      item.onDetailsTap_();
      Polymer.dom.flush();
      isActiveView(Page.DETAILS);

      // Switch: detail view -> keyboard shortcuts.
      MockInteractions.tap(manager.$.sidebar.$['sections-shortcuts']);
      Polymer.dom.flush();
      isActiveView(Page.SHORTCUTS);
    });

    test(assert(TestNames.UrlNavigationToDetails), function() {
      isActiveView(Page.DETAILS);
      var detailsView = manager.$['details-view'];
      expectEquals('ldnnhddmnhbkjipkidpdiheffobcpfmf', detailsView.data.id);
    });

    test(assert(TestNames.UpdateItemData), function() {
      var oldDescription = 'old description';
      var newDescription = 'new description';
      var extension = extension_test_util.createExtensionInfo(
          {description: oldDescription});
      var secondExtension = extension_test_util.createExtensionInfo({
        description: 'irrelevant',
        id: 'b'.repeat(32),
      });
      manager.addItem(extension);
      manager.addItem(secondExtension);
      var data = manager.extensions[0];
      // TODO(scottchen): maybe testing too many things in a single unit test.
      extensions.navigation.navigateTo(
          {page: Page.DETAILS, extensionId: extension.id});
      Polymer.dom.flush();
      var detailsView = manager.$['details-view'];
      expectEquals(extension.id, detailsView.data.id);
      expectEquals(oldDescription, detailsView.data.description);
      expectEquals(
          oldDescription,
          detailsView.$$('.section .section-content').textContent.trim());

      var extensionCopy = Object.assign({}, extension);
      extensionCopy.description = newDescription;
      manager.updateItem(extensionCopy);
      expectEquals(extension.id, detailsView.data.id);
      expectEquals(newDescription, detailsView.data.description);
      expectEquals(
          newDescription,
          detailsView.$$('.section .section-content').textContent.trim());

      // Updating a different extension shouldn't have any impact.
      var secondExtensionCopy = Object.assign({}, secondExtension);
      secondExtensionCopy.description = 'something else';
      manager.updateItem(secondExtensionCopy);
      expectEquals(extension.id, detailsView.data.id);
      expectEquals(newDescription, detailsView.data.description);
      expectEquals(
          newDescription,
          detailsView.$$('.section .section-content').textContent.trim());

    });
  });

  return {
    TestNames: TestNames,
  };
});
