// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-list>', function() {
  var list;
  var store;

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(createFolder(
          '0',
          [
            createItem('1'),
            createFolder('3', []),
            createItem('5'),
            createItem('7'),
          ])),
      selectedFolder: '0',
    });
    bookmarks.Store.instance_ = store;

    list = document.createElement('bookmarks-list');
    replaceBody(list);
    Polymer.dom.flush();
  });

  test('renders correct <bookmark-item> elements', function() {
    var items = list.root.querySelectorAll('bookmarks-item');
    var ids = Array.from(items).map((item) => item.itemId);

    assertDeepEquals(['1', '3', '5', '7'], ids);
  });

  test('shift-selects multiple items', function() {
    var items = list.root.querySelectorAll('bookmarks-item');

    customClick(items[0]);

    assertEquals('select-items', store.lastAction.name);
    assertFalse(store.lastAction.add);
    assertEquals('1', store.lastAction.anchor);
    assertDeepEquals(['1'], store.lastAction.items);

    store.data.selection.anchor = '1';
    customClick(items[2], {shiftKey: true, ctrlKey: true});

    assertEquals('select-items', store.lastAction.name);
    assertTrue(store.lastAction.add);
    assertEquals('5', store.lastAction.anchor);
    assertDeepEquals(['1', '3', '5'], store.lastAction.items);
  });

  test('deselects items on click outside of card', function() {
    customClick(list);
    assertEquals('deselect-items', store.lastAction.name);
  });
});
