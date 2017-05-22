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
    action = bookmarks.actions.selectItem('2', store.data, {
      clear: false,
      range: false,
      toggle: false,
    });
    var expected = {
      name: 'select-items',
      items: ['2'],
      clear: false,
      toggle: false,
      anchor: '2',
    };
    assertDeepEquals(expected, action);
  });

  test('can shift-select in regular list', function() {
    store.data.selection.anchor = '2';
    action = bookmarks.actions.selectItem('4', store.data, {
      clear: true,
      range: true,
      toggle: false,
    });

    assertDeepEquals(['2', '8', '4'], action.items);
    // Shift-selection doesn't change anchor.
    assertDeepEquals('2', action.anchor);
  });

  test('can shift-select in search results', function() {
    store.data.selectedFolder = null;
    store.data.search = {
      term: 'test',
      results: ['1', '4', '8'],
      inProgress: false,
    };
    store.data.selection.anchor = '8';

    action = bookmarks.actions.selectItem('4', store.data, {
      clear: true,
      range: true,
      toggle: false,
    });

    assertDeepEquals(['4', '8'], action.items);
  });

  test('selects the item when the anchor is missing', function() {
    // Anchor hasn't been set yet.
    store.data.selection.anchor = null;

    action = bookmarks.actions.selectItem('4', store.data, {
      clear: false,
      range: true,
      toggle: false,
    });
    assertEquals('4', action.anchor);
    assertDeepEquals(['4'], action.items);

    // Anchor set to an item which doesn't exist.
    store.data.selection.anchor = '42';

    action = bookmarks.actions.selectItem('8', store.data, {
      clear: false,
      range: true,
      toggle: false,
    });
    assertEquals('8', action.anchor);
    assertDeepEquals(['8'], action.items);
  });
});

test('selectFolder prevents selecting invalid nodes', function() {
  var nodes = testTree(createFolder('1', [
    createItem('2'),
  ]));

  var action = bookmarks.actions.selectFolder(ROOT_NODE_ID, nodes);
  assertEquals(null, action);

  action = bookmarks.actions.selectFolder('2', nodes);
  assertEquals(null, action);

  action = bookmarks.actions.selectFolder('42', nodes);
  assertEquals(null, action);

  action = bookmarks.actions.selectFolder('1', nodes);
  assertEquals('select-folder', action.name);
  assertEquals('1', action.id);
});
