// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-sidebar>', function() {
  var sidebar;
  var TEST_TREE;

  setup(function() {
    TEST_TREE = createFolder('0', [
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
            createItem('6'),
          ]),
      createFolder('7', []),
      createFolder('8', []),
    ]);

    setupTreeForUITests(TEST_TREE);
    sidebar = document.createElement('bookmarks-sidebar');
    replaceBody(sidebar);
    sidebar.rootFolders = TEST_TREE.children;
  });

  test('selecting and deselecting folders fires event', function() {
    var firedId;
    document.addEventListener('selected-folder-changed', function(e) {
      firedId = /** @type {string} */ (e.detail);
    });

    Polymer.dom.flush();
    var rootFolders = sidebar.$['folder-tree'].children;
    var firstGen = rootFolders[0].$['descendants'].querySelectorAll(
        'bookmarks-folder-node');
    var secondGen =
        firstGen[0].$['descendants'].querySelectorAll('bookmarks-folder-node');

    // Select nested folder.
    firedId = '';
    MockInteractions.tap(secondGen[0].$['folder-label']);
    assertEquals(secondGen[0].item.id, firedId);

    // Select folder in a separate subtree.
    firedId = '';
    MockInteractions.tap(rootFolders[1].$['folder-label']);
    assertEquals(rootFolders[1].item.id, firedId);
  });
});
