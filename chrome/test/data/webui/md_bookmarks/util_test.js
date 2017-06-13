// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('util', function() {
  test('getDescendants collects all children', function() {
    var nodes = testTree(createFolder('0', [
      createFolder('1', []),
      createFolder(
          '2',
          [
            createItem('3'),
            createFolder(
                '4',
                [
                  createItem('6'),
                  createFolder('7', []),
                ]),
            createItem('5'),
          ]),
    ]));

    var descendants = bookmarks.util.getDescendants(nodes, '1');
    assertDeepEquals(['1'], normalizeSet(descendants));

    descendants = bookmarks.util.getDescendants(nodes, '4');
    assertDeepEquals(['4', '6', '7'], normalizeSet(descendants));

    descendants = bookmarks.util.getDescendants(nodes, '2');
    assertDeepEquals(['2', '3', '4', '5', '6', '7'], normalizeSet(descendants));

    descendants = bookmarks.util.getDescendants(nodes, '42');
    assertDeepEquals([], normalizeSet(descendants));
  });

  test('removeIdsFromMap', function() {
    var map = {
      '1': true,
      '2': false,
      '4': true,
    };

    var nodes = new Set([2, 3, 4]);

    var newMap = bookmarks.util.removeIdsFromMap(map, nodes);

    assertEquals(undefined, newMap['2']);
    assertEquals(undefined, newMap['4']);
    assertTrue(newMap['1']);

    // Should not have changed the input map.
    assertFalse(map['2']);
  });

  test('removeIdsFromSet', function() {
    var set = new Set(['1', '3', '5']);
    var toRemove = new Set(['1', '2', '3']);

    var newSet = bookmarks.util.removeIdsFromSet(set, toRemove);
    assertDeepEquals(['5'], normalizeSet(newSet));
  });

  test('canEditNode and canReorderChildren', function() {
    var store = new bookmarks.TestStore({
      nodes: testTree(
          createFolder(
              '1',
              [
                createItem('11'),
              ]),
          createFolder(
              '4',
              [
                createItem('41', {unmodifiable: 'managed'}),
              ],
              {unmodifiable: 'managed'})),
    });

    // Top-level folders are unmodifiable, but their children can be changed.
    assertFalse(bookmarks.util.canEditNode(store.data, '1'));
    assertTrue(bookmarks.util.canReorderChildren(store.data, '1'));

    // Managed folders are entirely unmodifiable.
    assertFalse(bookmarks.util.canEditNode(store.data, '4'));
    assertFalse(bookmarks.util.canReorderChildren(store.data, '4'));
    assertFalse(bookmarks.util.canEditNode(store.data, '41'));
    assertFalse(bookmarks.util.canReorderChildren(store.data, '41'));

    // Regular nodes are modifiable.
    assertTrue(bookmarks.util.canEditNode(store.data, '11'));
    assertTrue(bookmarks.util.canReorderChildren(store.data, '11'));

    // When editing is disabled globally, everything is unmodifiable.
    store.data.prefs.canEdit = false;

    assertFalse(bookmarks.util.canEditNode(store.data, '1'));
    assertFalse(bookmarks.util.canReorderChildren(store.data, '1'));

    assertFalse(bookmarks.util.canEditNode(store.data, '41'));
    assertFalse(bookmarks.util.canReorderChildren(store.data, '41'));

    assertFalse(bookmarks.util.canEditNode(store.data, '11'));
    assertFalse(bookmarks.util.canReorderChildren(store.data, '11'));
  });
});
