// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('selection state', function() {
  var state;
  var action;

  function select(items, anchor, add) {
    return {
      name: 'select-items',
      add: add,
      anchor: anchor,
      items: items,
    };
  }

  setup(function() {
    state = {
      anchor: null,
      items: {},
    };
  });

  test('can select an item', function() {
    action = select(['1'], '1', false);
    state = bookmarks.SelectionState.updateSelection(state, action);

    assertDeepEquals({'1': true}, state.items);
    assertEquals('1', state.anchor);

    // Replace current selection.
    action = select(['2'], '2', false);
    state = bookmarks.SelectionState.updateSelection(state, action);
    assertDeepEquals({'2': true}, state.items);
    assertEquals('2', state.anchor);

    // Add to current selection.
    action = select(['3'], '3', true);
    state = bookmarks.SelectionState.updateSelection(state, action);
    assertDeepEquals({'2': true, '3': true}, state.items);
    assertEquals('3', state.anchor);
  });

  test('can select multiple items', function() {
    action = select(['1', '2', '3'], '3', false);
    state = bookmarks.SelectionState.updateSelection(state, action);
    assertDeepEquals({'1': true, '2': true, '3': true}, state.items);

    action = select(['3', '4'], '4', true);
    state = bookmarks.SelectionState.updateSelection(state, action);
    assertDeepEquals({'1': true, '2': true, '3': true, '4': true}, state.items);
  });

  test('is cleared when selected folder changes', function() {
    action = select(['1', '2', '3'], '3', false);
    state = bookmarks.SelectionState.updateSelection(state, action);

    action = bookmarks.actions.selectFolder('2');
    state = bookmarks.SelectionState.updateSelection(state, action);
    assertDeepEquals({}, state.items);
  });

  test('is cleared when search finished', function() {
    action = select(['1', '2', '3'], '3', false);
    state = bookmarks.SelectionState.updateSelection(state, action);

    action = bookmarks.actions.setSearchResults(['2']);
    state = bookmarks.SelectionState.updateSelection(state, action);
    assertDeepEquals({}, state.items);
  });

  test('is cleared when search cleared', function() {
    action = select(['1', '2', '3'], '3', false);
    state = bookmarks.SelectionState.updateSelection(state, action);

    action = bookmarks.actions.clearSearch();
    state = bookmarks.SelectionState.updateSelection(state, action);
    assertDeepEquals({}, state.items);
  });
});

suite('closed folder state', function() {
  var nodes;
  var state;
  var action;

  setup(function() {
    nodes = testTree(
        createFolder(
            '1',
            [
              createFolder('2', []),
              createItem('3'),
            ]),
        createFolder('4', []));
    state = {};
  });

  test('toggle folder open state', function() {
    action = bookmarks.actions.changeFolderOpen('2', false);
    state =
        bookmarks.ClosedFolderState.updateClosedFolders(state, action, nodes);
    assertFalse(!!state['1']);
    assertTrue(state['2']);
  });

  test('select folder with closed parent', function() {
    // Close '1'
    action = bookmarks.actions.changeFolderOpen('1', false);
    state =
        bookmarks.ClosedFolderState.updateClosedFolders(state, action, nodes);
    assertTrue(state['1']);
    assertFalse(!!state['2']);

    // Should re-open when '2' is selected.
    action = bookmarks.actions.selectFolder('2');
    state =
        bookmarks.ClosedFolderState.updateClosedFolders(state, action, nodes);
    assertFalse(!!state['1']);
  });

  test('move nodes in a closed folder', function() {
    // Moving bookmark items should not open folders.
    state = {'1': true};
    action = bookmarks.actions.moveBookmark('3', '1', 1, '1', 0);
    state =
        bookmarks.ClosedFolderState.updateClosedFolders(state, action, nodes);

    assertTrue(state['1']);

    // Moving folders should open their parents.
    state = {'1': true, '2': true};
    action = bookmarks.actions.moveBookmark('4', '2', 0, '0', 1);
    state =
        bookmarks.ClosedFolderState.updateClosedFolders(state, action, nodes);
    assertFalse(!!state['1']);
    assertFalse(!!state['2']);
  });
});

suite('selected folder', function() {
  var nodes;
  var state;
  var action;

  setup(function() {
    nodes = testTree(createFolder('1', [
      createFolder('2', []),
    ]));

    state = '1';
  });

  test('updates from selectFolder action', function() {
    action = bookmarks.actions.selectFolder('2');
    state = bookmarks.SelectedFolderState.updateSelectedFolder(
        state, action, nodes);
    assertEquals('2', state);
  });

  test('updates when parent of selected folder is closed', function() {
    action = bookmarks.actions.selectFolder('2');
    state = bookmarks.SelectedFolderState.updateSelectedFolder(
        state, action, nodes);

    action = bookmarks.actions.changeFolderOpen('1', false);
    state = bookmarks.SelectedFolderState.updateSelectedFolder(
        state, action, nodes);
    assertEquals('1', state);
  });
});

suite('node state', function() {
  var state;
  var action;

  setup(function() {
    state = testTree(
        createFolder(
            '1',
            [
              createItem('2', {title: 'a', url: 'a.com'}),
              createItem('3'),
              createFolder('4', []),
            ]),
        createFolder('5', []));
  });

  test('updates when a node is edited', function() {
    action = bookmarks.actions.editBookmark('2', {title: 'b'});
    state = bookmarks.NodeState.updateNodes(state, action);

    assertEquals('b', state['2'].title);
    assertEquals('a.com', state['2'].url);

    action = bookmarks.actions.editBookmark('2', {title: 'c', url: 'c.com'});
    state = bookmarks.NodeState.updateNodes(state, action);

    assertEquals('c', state['2'].title);
    assertEquals('c.com', state['2'].url);

    action = bookmarks.actions.editBookmark('4', {title: 'folder'});
    state = bookmarks.NodeState.updateNodes(state, action);

    assertEquals('folder', state['4'].title);
    assertEquals(undefined, state['4'].url);

    // Cannot edit URL of a folder:
    action = bookmarks.actions.editBookmark('4', {url: 'folder.com'});
    state = bookmarks.NodeState.updateNodes(state, action);

    assertEquals('folder', state['4'].title);
    assertEquals(undefined, state['4'].url);
  });

  test('updates when a node is deleted', function() {
    action = bookmarks.actions.removeBookmark('3', '1', 1);
    state = bookmarks.NodeState.updateNodes(state, action);

    assertDeepEquals(['2', '4'], state['1'].children);

    // TODO(tsergeant): Deleted nodes should be removed from the nodes map
    // entirely.
  });

  test('updates when a node is moved', function() {
    // Move within the same folder backwards.
    action = bookmarks.actions.moveBookmark('3', '1', 0, '1', 1);
    state = bookmarks.NodeState.updateNodes(state, action);

    assertDeepEquals(['3', '2', '4'], state['1'].children);

    // Move within the same folder forwards.
    action = bookmarks.actions.moveBookmark('3', '1', 2, '1', 0);
    state = bookmarks.NodeState.updateNodes(state, action);

    assertDeepEquals(['2', '4', '3'], state['1'].children);

    // Move between different folders.
    action = bookmarks.actions.moveBookmark('4', '5', 0, '1', 1);
    state = bookmarks.NodeState.updateNodes(state, action);

    assertDeepEquals(['2', '3'], state['1'].children);
    assertDeepEquals(['4'], state['5'].children);
  });
});

suite('search state', function() {
  var state;

  setup(function() {
    // Search touches a few different things, so we test using the entire state:
    state = bookmarks.util.createEmptyState();
    state.nodes = testTree(createFolder('1', [
      createFolder(
          '2',
          [
            createItem('3'),
          ]),
    ]));
  });

  test('updates when search is started and finished', function() {
    var action;

    action = bookmarks.actions.selectFolder('2');
    state = bookmarks.reduceAction(state, action);

    action = bookmarks.actions.setSearchTerm('test');
    state = bookmarks.reduceAction(state, action);

    assertEquals('test', state.search.term);
    assertTrue(state.search.inProgress);

    // UI should not have changed yet:
    assertEquals('2', state.selectedFolder);
    assertDeepEquals(['3'], bookmarks.util.getDisplayedList(state));

    action = bookmarks.actions.setSearchResults(['2', '3']);
    var searchedState = bookmarks.reduceAction(state, action);

    assertFalse(searchedState.search.inProgress);

    // UI changes once search results arrive:
    assertEquals(null, searchedState.selectedFolder);
    assertDeepEquals(
        ['2', '3'], bookmarks.util.getDisplayedList(searchedState));

    // Case 1: Clear search by setting an empty search term.
    action = bookmarks.actions.setSearchTerm('');
    var clearedState = bookmarks.reduceAction(searchedState, action);

    assertEquals('1', clearedState.selectedFolder);
    assertDeepEquals(['2'], bookmarks.util.getDisplayedList(clearedState));
    assertEquals('', clearedState.search.term);
    assertDeepEquals([], clearedState.search.results);

    // Case 2: Clear search by selecting a new folder.
    action = bookmarks.actions.selectFolder('2');
    var selectedState = bookmarks.reduceAction(searchedState, action);

    assertEquals('2', selectedState.selectedFolder);
    assertDeepEquals(['3'], bookmarks.util.getDisplayedList(selectedState));
    assertEquals('', selectedState.search.term);
    assertDeepEquals([], selectedState.search.results);
  });
});
