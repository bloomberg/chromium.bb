// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-toolbar>', function() {
  var toolbar;
  var store;
  var commandManager;

  suiteSetup(function() {
    chrome.bookmarkManagerPrivate.removeTrees = function() {};
  });

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(createFolder('1', [
        createItem('2'),
        createItem('3'),
      ])),
      selection: {
        items: new Set(),
        anchor: null,
      },
    });
    bookmarks.Store.instance_ = store;

    toolbar = document.createElement('bookmarks-toolbar');
    replaceBody(toolbar);

    commandManager = new TestCommandManager();
    document.body.appendChild(commandManager);
  });

  test('selecting multiple items shows toolbar overlay', function() {
    assertFalse(toolbar.showSelectionOverlay);

    store.data.selection.items = new Set(['2']);
    store.notifyObservers();
    assertFalse(toolbar.showSelectionOverlay);

    store.data.selection.items = new Set(['2', '3']);
    store.notifyObservers();
    assertTrue(toolbar.showSelectionOverlay);
  });

  test('clicking overlay delete button triggers a delete command', function() {
    store.data.selection.items = new Set(['2', '3']);
    store.notifyObservers();

    Polymer.dom.flush();
    MockInteractions.tap(
        toolbar.$$('cr-toolbar-selection-overlay').deleteButton);

    commandManager.assertLastCommand(Command.DELETE, ['2', '3']);
  });
});
