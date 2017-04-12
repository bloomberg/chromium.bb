// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for action creators that depend on the page state
 * and/or have non-trivial logic.
 */

suite('selectItem', function() {
  var store;
  var action;

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(createFolder(
          '1',
          [
            createItem('2'),
            createItem('8'),
            createFolder('4', []),
            createItem('6'),
          ])),
      selectedFolder: '1',
    });
  });

  test('can select single item', function() {
    action = bookmarks.actions.selectItem('2', true, false, store.data);
    var expected = {
      name: 'select-items',
      items: ['2'],
      add: true,
      anchor: '2',
    };
    assertDeepEquals(expected, action);
  });

  test('can shift-select in regular list', function() {
    store.data.selection.anchor = '2';
    action = bookmarks.actions.selectItem('4', false, true, store.data);

    assertDeepEquals(['2', '8', '4'], action.items);
    assertDeepEquals('4', action.anchor);
  });

  test('can shift-select in search results', function() {
    store.data.selectedFolder = null;
    store.data.search = {
      term: 'test',
      results: ['1', '4', '8'],
      inProgress: false,
    };
    store.data.selection.anchor = '8';

    action = bookmarks.actions.selectItem('4', false, true, store.data);

    assertDeepEquals(['4', '8'], action.items);
  });

  test('selects the item when the anchor is missing', function() {
    // Anchor hasn't been set yet.
    store.data.selection.anchor = null;

    action = bookmarks.actions.selectItem('4', true, true, store.data);
    assertEquals('4', action.anchor);
    assertDeepEquals(['4'], action.items);

    // Anchor set to an item which doesn't exist.
    store.data.selection.anchor = '42';

    action = bookmarks.actions.selectItem('8', true, true, store.data);
    assertEquals('8', action.anchor);
    assertDeepEquals(['8'], action.items);
  });
});
