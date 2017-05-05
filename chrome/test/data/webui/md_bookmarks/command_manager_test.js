// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-command-manager>', function() {
  var commandManager;
  var store;

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(createFolder(
          '1',
          [
            createFolder('11', []),
            createFolder('12', []),
            createItem('13'),
          ])),
    });
    bookmarks.Store.instance_ = store;

    commandManager = document.createElement('bookmarks-command-manager');
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
});
