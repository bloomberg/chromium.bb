// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('closed folder state', function() {
  var nodes;
  var initialState;

  setup(function() {
    nodes = testTree(createFolder('0', [
      createFolder('1', []),
    ]));
    initialState = {};
  });

  test('toggle folder open state', function() {
    var action = bookmarks.actions.changeFolderOpen('1', false);
    var nextState = bookmarks.ClosedFolderState.updateClosedFolders(
        initialState, action, nodes);
    assertTrue(nextState['1']);
    assertFalse(!!nextState['0']);
  });

  test('select folder with closed parent', function() {
    var action;
    var nextState;
    // Close '0'
    action = bookmarks.actions.changeFolderOpen('0', false);
    nextState = bookmarks.ClosedFolderState.updateClosedFolders(
        initialState, action, nodes);
    assertTrue(nextState['0']);

    // Should re-open when '1' is selected.
    action = bookmarks.actions.selectFolder('1');
    nextState = bookmarks.ClosedFolderState.updateClosedFolders(
        nextState, action, nodes);
    assertFalse(nextState['0']);
  });
});

suite('selected folder', function() {
  var nodes;
  var initialState;

  setup(function() {
    nodes = testTree(createFolder('0', [
      createFolder('1', []),
    ]));

    initialState = '0';
  });

  test('updates from selectFolder action', function() {
    var action = bookmarks.actions.selectFolder('1');
    var newState = bookmarks.SelectedFolderState.updateSelectedFolder(
        initialState, action, nodes);
    assertEquals('1', newState);
  });

  test('updates when parent of selected folder is closed', function() {
    var action;
    var newState;

    action = bookmarks.actions.selectFolder('1');
    newState = bookmarks.SelectedFolderState.updateSelectedFolder(
        initialState, action, nodes);

    action = bookmarks.actions.changeFolderOpen('0', false);
    newState = bookmarks.SelectedFolderState.updateSelectedFolder(
        newState, action, nodes);
    assertEquals('0', newState);
  });
});

suite('node state', function() {
  var initialState;

  setup(function() {
    initialState = testTree(createFolder('0', [
      createFolder(
          '1',
          [
            createItem('2', {title: 'a', url: 'a.com'}),
            createItem('3'),
            createFolder('4', []),
          ]),
      createFolder('5', []),
    ]));
  });

  test('updates when a node is edited', function() {
    var action = bookmarks.actions.editBookmark('2', {title: 'b'});
    var nextState = bookmarks.NodeState.updateNodes(initialState, action);

    assertEquals('b', nextState['2'].title);
    assertEquals('a.com', nextState['2'].url);

    action = bookmarks.actions.editBookmark('2', {title: 'c', url: 'c.com'});
    nextState = bookmarks.NodeState.updateNodes(nextState, action);

    assertEquals('c', nextState['2'].title);
    assertEquals('c.com', nextState['2'].url);

    action = bookmarks.actions.editBookmark('4', {title: 'folder'});
    nextState = bookmarks.NodeState.updateNodes(nextState, action);

    assertEquals('folder', nextState['4'].title);
    assertEquals(undefined, nextState['4'].url);

    // Cannot edit URL of a folder:
    action = bookmarks.actions.editBookmark('4', {url: 'folder.com'});
    nextState = bookmarks.NodeState.updateNodes(nextState, action);

    assertEquals('folder', nextState['4'].title);
    assertEquals(undefined, nextState['4'].url);
  });

  test('updates when a node is deleted', function() {
    var action = bookmarks.actions.removeBookmark('3', '1', 1);
    var nextState = bookmarks.NodeState.updateNodes(initialState, action);

    assertDeepEquals(['2', '4'], nextState['1'].children);

    // TODO(tsergeant): Deleted nodes should be removed from the nodes map
    // entirely.
  });
});
