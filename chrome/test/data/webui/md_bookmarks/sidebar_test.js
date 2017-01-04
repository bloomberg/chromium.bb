// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-sidebar>', function() {
  var sidebar;
  var TEST_TREE;

  setup(function() {
    TEST_TREE = [
      {
        id: '0',
        isSelected: true,
        children: [
          {
            id: '1',
            isSelected: false,
            children: [
              {id: '2', isSelected: false, children: []},
              {id: '3', isSelected: false, children: []},
            ],
          },
          {id: '4', url: 'link4'},
          {id: '5', url: 'link5'},
        ],
      },
      {id: '6', isSelected: false, children: []},
      {id: '7', isSelected: false, children: []}
    ];

    sidebar = document.createElement('bookmarks-sidebar');
    replaceBody(sidebar);
    sidebar.rootFolders = TEST_TREE;
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
