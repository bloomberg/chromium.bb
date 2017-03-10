// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-sidebar>', function() {
  var sidebar;
  var store;

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(createFolder('0', [
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
        createFolder('7', []),
      ])),
    });
    bookmarks.Store.instance_ = store;

    sidebar = document.createElement('bookmarks-sidebar');
    replaceBody(sidebar);
    Polymer.dom.flush();
  });

  test('selecting and deselecting folders dispatches action', function() {
    var rootFolders = sidebar.$['folder-tree'].children;
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
});
