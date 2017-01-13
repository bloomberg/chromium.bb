// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-store>', function() {
  var store;
  var TEST_TREE;

  setup(function() {
    TEST_TREE = createFolder('0', [
      createFolder(
          '1',
          [
            createItem('2', {url: 'link2'}),
            createFolder('3', []),
          ]),
      createItem('4', {url: 'link4'}),
      createItem('5', {url: 'link5'}),
    ]);

    store = document.createElement('bookmarks-store');
    replaceBody(store);
    store.setupStore_(TEST_TREE);
  });

  test('initNodes inserts nodes into idToNodeMap', function(){
    assertEquals(TEST_TREE, store.idToNodeMap_['0']);
    assertEquals(TEST_TREE.children[0], store.idToNodeMap_['1']);
    assertEquals(TEST_TREE.children[0].children[0], store.idToNodeMap_['2']);
    assertEquals(TEST_TREE.children[0].children[1], store.idToNodeMap_['3']);
    assertEquals(TEST_TREE.children[1], store.idToNodeMap_['4']);
    assertEquals(TEST_TREE.children[2], store.idToNodeMap_['5']);
  });

  test('changing selectedId changes the displayedList', function(){
    store.selectedId = '0';
    assertEquals(TEST_TREE.children, store.displayedList);
    store.selectedId = '1';
    assertEquals(TEST_TREE.children[0].children, store.displayedList);
    store.selectedId = '2';
    assertEquals(
        TEST_TREE.children[0].children[0].children, store.displayedList);
    store.selectedId = '3';
    assertEquals(
        TEST_TREE.children[0].children[1].children, store.displayedList);
    store.selectedId = '4';
    assertEquals(TEST_TREE.children[1].children, store.displayedList);
    store.selectedId = '5';
    assertEquals(TEST_TREE.children[2].children, store.displayedList);
  });

  test('correct paths generated for nodes', function() {
    var TEST_PATHS = {
      '0': 'rootNode',
      '1': 'rootNode.children.#0',
      '2': 'rootNode.children.#0.children.#0',
      '3': 'rootNode.children.#0.children.#1',
      '4': 'rootNode.children.#1',
      '5': 'rootNode.children.#2',
    };

    for (var id in store.idToNodeMap_)
      assertEquals(TEST_PATHS[id], store.idToNodeMap_[id].path);
  });

  test('store updates on selected event', function() {
    // First child of root is selected by default.
    assertEquals('1', store.selectedId);
    assertTrue(store.idToNodeMap_['1'].isSelected);

    // Selecting a selected folder doesn't deselect it.
    store.fire('selected-folder-changed', '1');
    assertEquals('1', store.selectedId);
    assertTrue(store.idToNodeMap_['1'].isSelected);

    // Select a deeply nested descendant.
    store.fire('selected-folder-changed', '3');
    assertEquals('3', store.selectedId);
    assertTrue(store.idToNodeMap_['3'].isSelected);
    assertFalse(store.idToNodeMap_['1'].isSelected);

    // Select a folder in separate subtree.
    store.fire('selected-folder-changed', '5');
    assertEquals('5', store.selectedId);
    assertTrue(store.idToNodeMap_['5'].isSelected);
    assertFalse(store.idToNodeMap_['3'].isSelected);
  });

  test('store updates on open and close', function() {
    // All folders are open by default.
    for (var id in store.idToNodeMap_) {
      if (store.idToNodeMap_[id].url)
        continue;

      assertTrue(store.idToNodeMap_[id].isOpen);
    }

    // Closing a folder doesn't close any descendants.
    store.fire('folder-open-changed', {id: '1', open: false});
    assertFalse(store.idToNodeMap_['1'].isOpen);
    assertTrue(store.idToNodeMap_['3'].isOpen);
    store.fire('folder-open-changed', {id: '1', open: true});

    // Closing an ancestor folder of a selected folder selects the ancestor.
    store.fire('selected-folder-changed', '3');
    store.fire('folder-open-changed', {id: '1', open: false});
    assertFalse(store.idToNodeMap_['1'].isOpen);
    assertEquals('1', store.selectedId);
    assertTrue(store.idToNodeMap_['1'].isSelected);
    assertFalse(store.idToNodeMap_['3'].isSelected);
  });

  test('deleting a node updates the tree', function() {
    // Remove an empty folder/bookmark.
    store.onBookmarkRemoved_('4', {parentId: '0', index: '1'});

    // Check the tree is correct.
    assertEquals('5', store.rootNode.children[1].id);

    // idToNodeMap_ has been updated.
    assertEquals(undefined, store.idToNodeMap_['4']);
    assertEquals(store.rootNode.children[1], store.idToNodeMap_['5']);

    // Paths have been updated.
    var TEST_PATHS = {
      '0': 'rootNode',
      '1': 'rootNode.children.#0',
      '2': 'rootNode.children.#0.children.#0',
      '3': 'rootNode.children.#0.children.#1',
      '5': 'rootNode.children.#1',
    };

    for (var id in store.idToNodeMap_)
      assertEquals(TEST_PATHS[id], store.idToNodeMap_[id].path);

    // Remove a folder with children.
    store.onBookmarkRemoved_('1', {parentId: '0', index: '0'});

    // Check the tree is correct.
    assertEquals('5', store.rootNode.children[0].id);

    // idToNodeMap_ has been updated.
    assertEquals(undefined, store.idToNodeMap_['1']);
    assertEquals(undefined, store.idToNodeMap_['2']);
    assertEquals(undefined, store.idToNodeMap_['3']);
    assertEquals(undefined, store.idToNodeMap_['4']);
    assertEquals(store.rootNode.children[0], store.idToNodeMap_['5']);

    // Paths have been updated.
    TEST_PATHS = {
      '0': 'rootNode',
      '5': 'rootNode.children.#0',
    };

    for (var id in store.idToNodeMap_)
      assertEquals(TEST_PATHS[id], store.idToNodeMap_[id].path);
  });

  test('selectedId updates after removing a selected folder', function() {
    // Selected folder gets removed.
    store.selectedId = '2';
    store.onBookmarkRemoved_('2', {parentId:'1', index:'0'});
    assertTrue(store.idToNodeMap_['1'].isSelected);
    assertEquals('1', store.selectedId);

    // A folder with selected folder in it gets removed.
    store.selectedId = '3';
    store.onBookmarkRemoved_('1', {parentId:'0', index:'0'});
    assertTrue(store.idToNodeMap_['0'].isSelected);
    assertEquals('0', store.selectedId);
  });

  test('displayedList updates after searchTerm changes', function() {
      var SEARCH_RESULTS = [
        'cat',
        'apple',
        'Paris',
      ];

      chrome.bookmarks.search = function(searchTerm, callback) {
        callback(SEARCH_RESULTS);
      };

      // Search for a non-empty string.
      store.searchTerm = 'a';
      assertFalse(store.rootNode.children[0].isSelected);
      assertEquals(null, store.selectedId);
      assertEquals(SEARCH_RESULTS, store.displayedList);

      // Clear the searchTerm.
      store.searchTerm = '';
      var defaultFolder = store.rootNode.children[0];
      assertTrue(defaultFolder.isSelected);
      assertEquals(defaultFolder.id, store.selectedId);
      assertEquals(defaultFolder.children, store.displayedList);

      // Search with no bookmarks returned.
      var EMPTY_RESULT = [];
      chrome.bookmarks.search = function(searchTerm, callback) {
        callback(EMPTY_RESULT);
      };
      store.searchTerm = 'asdf';
      assertEquals(EMPTY_RESULT, store.displayedList);
  });

  test('bookmark gets updated after editing', function() {
    // Edit title updates idToNodeMap_ properly.
    store.onBookmarkChanged_('4', {'title': 'test'});
    assertEquals('test', store.idToNodeMap_['4'].title);
    assertEquals('link4', store.idToNodeMap_['4'].url);

    // Edit url updates idToNodeMap_ properly.
    store.onBookmarkChanged_('5', {'url': 'http://www.google.com'});
    assertEquals('', store.idToNodeMap_['5'].title);
    assertEquals('http://www.google.com', store.idToNodeMap_['5'].url);

    // Edit url and title updates idToNodeMap_ properly.
    store.onBookmarkChanged_('2', {
      'title': 'test',
      'url': 'http://www.google.com',
    });
    assertEquals('test', store.idToNodeMap_['2'].title);
    assertEquals('http://www.google.com', store.idToNodeMap_['2'].url);
  });
});
