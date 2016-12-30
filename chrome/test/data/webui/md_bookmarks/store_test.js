// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-store>', function() {
  var store;
  var TEST_TREE = {
    id: '0',
    children: [
      {
        id: '1',
        children: [
          {id: '2', url: 'link2'},
          {id: '3', children: []}
        ]
      },
      {id: '4', url: 'link4'},
      {id: '5', url: 'link5'}
    ]
  };

  setup(function() {
    store = document.createElement('bookmarks-store');
    store.isTesting_ = true;
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

  test('changing selectedId changes the selectedNode', function(){
    store.selectedId = '0';
    assertEquals(TEST_TREE, store.selectedNode);
    store.selectedId = '1';
    assertEquals(TEST_TREE.children[0], store.selectedNode);
    store.selectedId = '2';
    assertEquals(TEST_TREE.children[0].children[0], store.selectedNode);
    store.selectedId = '3';
    assertEquals(TEST_TREE.children[0].children[1], store.selectedNode);
    store.selectedId = '4';
    assertEquals(TEST_TREE.children[1], store.selectedNode);
    store.selectedId = '5';
    assertEquals(TEST_TREE.children[2], store.selectedNode);
  });

  test('correct paths generated for nodes', function() {
    var TEST_PATHS = {
      '0': 'rootNode',
      '1': 'rootNode.children.0',
      '2': 'rootNode.children.0.children.0',
      '3': 'rootNode.children.0.children.1',
      '4': 'rootNode.children.1',
      '5': 'rootNode.children.2',
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
});
