// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-command-manager>', function() {
  var commandManager;
  var store;
  var lastCommand;
  var lastCommandIds;

  function assertLastCommand(command, ids) {
    assertEquals(lastCommand, command);
    if (ids)
      assertDeepEquals(normalizeSet(lastCommandIds), ids);
    lastCommand = null;
    lastCommandIds = null;
  }

  suiteSetup(function() {
    // Overwrite bookmarkManagerPrivate APIs which will crash if called with
    // fake data.
    chrome.bookmarkManagerPrivate.copy = function() {};
    chrome.bookmarkManagerPrivate.removeTrees = function() {};
  });

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(
          createFolder(
              '1',
              [
                createFolder(
                    '11',
                    [
                      createItem('111'),
                    ]),
                createFolder('12', []),
                createItem('13'),
              ]),
          createFolder('2', [])),
    });
    bookmarks.Store.instance_ = store;

    commandManager = document.createElement('bookmarks-command-manager');

    var realHandle = commandManager.handle.bind(commandManager);
    commandManager.handle = function(command, itemIds) {
      lastCommand = command;
      lastCommandIds = itemIds;
      realHandle(command, itemIds);
    };
    replaceBody(commandManager);
  });

  test('can only copy single URL items', function() {
    assertFalse(commandManager.canExecute(Command.COPY, new Set(['11'])));
    assertFalse(commandManager.canExecute(Command.COPY, new Set(['11', '13'])));
    assertTrue(commandManager.canExecute(Command.COPY, new Set(['13'])));
  });

  test('context menu hides invalid commands', function() {
    store.data.selection.items = new Set(['11', '13']);
    store.notifyObservers();

    commandManager.openCommandMenuAtPosition(0, 0);
    var commandHidden = {};
    commandManager.root.querySelectorAll('.dropdown-item').forEach(element => {
      commandHidden[element.getAttribute('command')] = element.hidden;
    });

    // With a folder and an item selected, the only available context menu item
    // is 'Delete'.
    assertTrue(commandHidden['edit']);
    assertTrue(commandHidden['copy']);
    assertFalse(commandHidden['delete']);
  });

  test('keyboard shortcuts trigger when valid', function() {
    var modifier = cr.isMac ? 'meta' : 'ctrl';

    store.data.selection.items = new Set(['13']);
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document, 67, modifier, 'c');
    assertLastCommand('copy', ['13']);

    // Doesn't trigger when a folder is selected.
    store.data.selection.items = new Set(['11']);
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document, 67, modifier, 'c');
    assertLastCommand(null);

    // Doesn't trigger when nothing is selected.
    store.data.selection.items = new Set();
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document, 67, modifier, 'c');
    assertLastCommand(null);
  });

  test('delete command triggers', function() {
    store.data.selection.items = new Set(['12', '13']);
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document, 46, '', 'Delete');
    assertLastCommand('delete', ['12', '13']);
  });

  test('edit command triggers', function() {
    var key = cr.isMac ? 'Enter' : 'F2';
    var keyCode = cr.isMac ? 13 : 113;

    store.data.selection.items = new Set(['11']);
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document, keyCode, '', key);
    assertLastCommand('edit', ['11']);
  });

  test('does not delete children at same time as ancestor', function() {
    var lastDelete = null;
    chrome.bookmarkManagerPrivate.removeTrees = function(idArray) {
      lastDelete = idArray.sort();
    };

    var parentAndChildren = new Set(['1', '2', '12', '111']);
    assertTrue(commandManager.canExecute(Command.DELETE, parentAndChildren));
    commandManager.handle(Command.DELETE, parentAndChildren);

    assertDeepEquals(['1', '2'], lastDelete);
  });
});
