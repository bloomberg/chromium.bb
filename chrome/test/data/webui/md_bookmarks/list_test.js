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

  test('selects individual items', function() {
    var items = list.root.querySelectorAll('bookmarks-item');

    customClick(items[0]);
    var expected = {
      name: 'select-items',
      add: false,
      anchor: '1',
      items: ['1'],
    };
    assertDeepEquals(expected, store.lastAction);

    customClick(items[2], {ctrlKey: true});
    expected.add = true;
    expected.anchor = '5';
    expected.items = ['5'];
    assertDeepEquals(expected, store.lastAction);
  });

  test('shift-selects multiple items', function() {
    var items = list.root.querySelectorAll('bookmarks-item');
    store.data.selection.anchor = '1';

    customClick(items[2], {shiftKey: true});

    assertEquals('select-items', store.lastAction.name);
    assertFalse(store.lastAction.add);
    assertEquals('5', store.lastAction.anchor);
    assertDeepEquals(['1', '3', '5'], store.lastAction.items);
  });

  test('selects the item when the anchor is missing', function() {
    var items = list.root.querySelectorAll('bookmarks-item');
    // Anchor hasn't been set yet:
    store.data.selection.anchor = null;

    customClick(items[0], {shiftKey: true});
    assertEquals('1', store.lastAction.anchor);
    assertDeepEquals(['1'], store.lastAction.items);

    // Anchor item doesn't exist:
    store.data.selection.anchor = '42';

    customClick(items[1], {shiftKey: true});

    assertEquals('3', store.lastAction.anchor);
    assertDeepEquals(['3'], store.lastAction.items);
  });
});
