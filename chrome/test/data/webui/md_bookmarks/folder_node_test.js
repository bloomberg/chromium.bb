// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-folder-node>', function() {
  var rootNode;
  var store;

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
    });
    bookmarks.Store.instance_ = store;

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
    firedId = '';
    MockInteractions.tap(secondGen[0].$['folder-label']);
    assertEquals('select-folder', store.lastAction.name);
    assertEquals(secondGen[0].itemId, store.lastAction.id);

    // Select folder in a separate subtree.
    firedId = '';
    MockInteractions.tap(rootFolders[1].$['folder-label']);
    assertEquals('select-folder', store.lastAction.name);
    assertEquals(rootFolders[1].itemId, store.lastAction.id);
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

    store.data.selectedFolder = '1';
    store.notifyObservers();

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
});
