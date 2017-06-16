// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-folder-node>', function() {
  var rootNode;
  var store;

  function getFolderNode(id) {
    return findFolderNode(rootNode, id);
  }

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(
          createFolder(
              '1',
              [
                createFolder(
                    '2',
                    [
                      createFolder('3', []),
                      createFolder('4', []),
                    ]),
                createItem('5'),
              ]),
          createFolder('7', [])),
      selectedFolder: '1',
    });
    store.replaceSingleton();

    rootNode = document.createElement('bookmarks-folder-node');
    rootNode.itemId = '0';
    rootNode.depth = -1;
    replaceBody(rootNode);
    Polymer.dom.flush();
  });

  test('selecting and deselecting folders dispatches action', function() {
    var rootFolders = rootNode.root.querySelectorAll('bookmarks-folder-node');
    var firstGen = rootFolders[0].$['descendants'].querySelectorAll(
        'bookmarks-folder-node');
    var secondGen =
        firstGen[0].$['descendants'].querySelectorAll('bookmarks-folder-node');

    // Select nested folder.
    MockInteractions.tap(secondGen[0].$['folder-label']);
    assertEquals('select-folder', store.lastAction.name);
    assertEquals(secondGen[0].itemId, store.lastAction.id);

    // Select folder in a separate subtree.
    MockInteractions.tap(rootFolders[1].$['folder-label']);
    assertEquals('select-folder', store.lastAction.name);
    assertEquals(rootFolders[1].itemId, store.lastAction.id);

    // Doesn't re-select if the folder is already selected.
    store.data.selectedFolder = '7';
    store.notifyObservers();
    store.resetLastAction();

    MockInteractions.tap(rootFolders[1].$['folder-label']);
    assertEquals(null, store.lastAction);
  });

  test('depth calculation', function() {
    var rootFolders = rootNode.root.querySelectorAll('bookmarks-folder-node');
    var firstGen = rootFolders[0].$['descendants'].querySelectorAll(
        'bookmarks-folder-node');
    var secondGen =
        firstGen[0].$['descendants'].querySelectorAll('bookmarks-folder-node');

    Array.prototype.forEach.call(rootFolders, function(f) {
      assertEquals(0, f.depth);
      assertEquals('0', f.style.getPropertyValue('--node-depth'));
    });
    Array.prototype.forEach.call(firstGen, function(f) {
      assertEquals(1, f.depth);
      assertEquals('1', f.style.getPropertyValue('--node-depth'));
    });
    Array.prototype.forEach.call(secondGen, function(f) {
      assertEquals(2, f.depth);
      assertEquals('2', f.style.getPropertyValue('--node-depth'));
    });
  });

  test('doesn\'t highlight selected folder while searching', function() {
    var rootFolders = rootNode.root.querySelectorAll('bookmarks-folder-node');

    assertEquals('1', rootFolders['0'].itemId);
    assertTrue(rootFolders['0'].isSelectedFolder_);

    store.data.search = {
      term: 'test',
      inProgress: false,
      results: ['3'],
    };
    store.notifyObservers();

    assertFalse(rootFolders['0'].isSelectedFolder_);
  });

  test('last visible descendant', function() {
    assertEquals('7', rootNode.getLastVisibleDescendant_().itemId);
    assertEquals('4', getFolderNode('1').getLastVisibleDescendant_().itemId);

    store.data.closedFolders = new Set('2');
    store.notifyObservers();

    assertEquals('2', getFolderNode('1').getLastVisibleDescendant_().itemId);
  });

  test('get node parent', function() {
    assertEquals(getFolderNode('0'), getFolderNode('1').getParentFolderNode_());
    assertEquals(getFolderNode('2'), getFolderNode('4').getParentFolderNode_());
    assertEquals(null, getFolderNode('0').getParentFolderNode_());
  });

  test('next/previous folder nodes', function() {
    function getNextChild(parentId, targetId, reverse) {
      return getFolderNode(parentId).getNextChild_(
          reverse, getFolderNode(targetId));
    }

    // Forwards.
    assertEquals('4', getNextChild('2', '3', false).itemId);
    assertEquals(null, getNextChild('2', '4', false));

    // Backwards.
    assertEquals(null, getNextChild('1', '2', true));
    assertEquals('3', getNextChild('2', '4', true).itemId);
    assertEquals('4', getNextChild('0', '7', true).itemId);

    // Skips closed folders.
    store.data.closedFolders = new Set('2');
    store.notifyObservers();

    assertEquals(null, getNextChild('1', '2', false));
    assertEquals('2', getNextChild('0', '7', true).itemId);
  });

  test('right click opens context menu', function() {
    var commandManager = new TestCommandManager();
    document.body.appendChild(commandManager);

    var node = getFolderNode('2');
    node.$.container.dispatchEvent(new MouseEvent('contextmenu'));

    assertDeepEquals(bookmarks.actions.selectFolder('2'), store.lastAction);
    commandManager.assertMenuOpenForIds(['2']);
  });
});
