// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-item. */
cr.define('extension_item_list_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Filtering: 'item list filtering',
    NoItemsMsg: 'empty item list',
    NoSearchResultsMsg: 'empty item list filtering results',
  };

  suite('ExtensionItemListTest', function() {
    /** @type {extensions.ItemList} */
    var itemList;
    var testVisible;

    // Initialize an extension item before each test.
    setup(function() {
      PolymerTest.clearBody();
      itemList = new extensions.ItemList();
      testVisible = extension_test_util.testVisible.bind(null, itemList);

      var createExt = extension_test_util.createExtensionInfo;
      var items = [
        createExt({name: 'Alpha', id: 'a'.repeat(32)}),
        createExt({name: 'Bravo', id: 'b'.repeat(32)}),
        createExt({name: 'Charlie', id: 'c'.repeat(32)})
      ];
      itemList.items = items;
      itemList.filter = '';
      document.body.appendChild(itemList);
    });

    test(assert(TestNames.Filtering), function() {
      var ironList = itemList.$.list;
      assert(ironList);

      // We should initially show all the items.
      expectEquals(3, ironList.items.length);

      // All items have an 'a'.
      itemList.filter = 'a';
      expectEquals(3, ironList.items.length);
      // Filtering is case-insensitive, so every item should be shown.
      itemList.filter = 'A';
      expectEquals(3, ironList.items.length);
      // Only 'Bravo' has a 'b'.
      itemList.filter = 'b';
      expectEquals(1, ironList.items.length);
      expectEquals('Bravo', ironList.items[0].name);
      // Test inner substring (rather than prefix).
      itemList.filter = 'lph';
      expectEquals(1, ironList.items.length);
      expectEquals('Alpha', ironList.items[0].name);
      // Test string with no matching items.
      itemList.filter = 'z';
      expectEquals(0, ironList.items.length);
      // A filter of '' should reset to show all items.
      itemList.filter = '';
      expectEquals(3, ironList.items.length);
    });

    test(assert(TestNames.NoItemsMsg), function() {
      testVisible('#no-items', false);
      testVisible('#no-search-results', false);

      itemList.items = [];
      testVisible('#no-items', true);
      testVisible('#no-search-results', false);
    });

    test(assert(TestNames.NoSearchResultsMsg), function() {
      testVisible('#no-items', false);
      testVisible('#no-search-results', false);

      itemList.filter = 'non-existent name';
      testVisible('#no-items', false);
      testVisible('#no-search-results', true);
    });
  });

  return {
    TestNames: TestNames,
  };
});
