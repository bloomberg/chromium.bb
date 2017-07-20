// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<bookmarks-command-manager>', function() {
  var commandManager;
  var store;
  var lastCommand;
  var lastCommandIds;

  suiteSetup(function() {
    // Overwrite bookmarkManagerPrivate APIs which will crash if called with
    // fake data.
    chrome.bookmarkManagerPrivate.copy = function() {};
    chrome.bookmarkManagerPrivate.removeTrees = function() {};
  });

  setup(function() {
    var bulkChildren = [];
    for (var i = 1; i <= 20; i++) {
      var id = '3' + i;
      bulkChildren.push(createItem(id, {url: `http://${id}/`}));
    }

    store = new bookmarks.TestStore({
      nodes: testTree(
          createFolder(
              '1',
              [
                createFolder(
                    '11',
                    [
                      createItem('111', {url: 'http://111/'}),
                    ]),
                createFolder(
                    '12',
                    [
                      createItem('121', {url: 'http://121/'}),
                      createFolder(
                          '122',
                          [
                            createItem('1221'),
                          ]),
                    ]),
                createItem('13', {url: 'http://13/'}),
              ]),
          createFolder(
              '2',
              [
                createFolder('21', []),
              ]),
          createFolder('3', bulkChildren)),
      selectedFolder: '1',
    });
    store.replaceSingleton();

    commandManager = new TestCommandManager();
    replaceBody(commandManager);
    document.body.appendChild(
        document.createElement('bookmarks-toast-manager'));
  });

  test('Copy URL is only active for single URL items', function() {
    assertFalse(commandManager.canExecute(Command.COPY_URL, new Set(['11'])));
    assertFalse(
        commandManager.canExecute(Command.COPY_URL, new Set(['11', '13'])));
    assertTrue(commandManager.canExecute(Command.COPY_URL, new Set(['13'])));
  });

  test('context menu hides invalid commands', function() {
    store.data.selection.items = new Set(['11', '13']);
    store.notifyObservers();

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.LIST);
    Polymer.dom.flush();

    var commandHidden = {};
    commandManager.root.querySelectorAll('.dropdown-item').forEach(element => {
      commandHidden[element.getAttribute('command')] = element.hidden;
    });

    // With a folder and an item selected, the only available context menu item
    // is 'Delete'.
    assertTrue(commandHidden[Command.EDIT]);
    assertTrue(commandHidden[Command.COPY_URL]);
    assertFalse(commandHidden[Command.DELETE]);
  });

  test('edit shortcut triggers when valid', function() {
    var key = cr.isMac ? 'Enter' : 'F2';

    store.data.selection.items = new Set(['13']);
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document.body, '', [], key);
    commandManager.assertLastCommand('edit', ['13']);

    // Doesn't trigger when multiple items are selected.
    store.data.selection.items = new Set(['11', '13']);
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document.body, '', [], key);
    commandManager.assertLastCommand(null);

    // Doesn't trigger when nothing is selected.
    store.data.selection.items = new Set();
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document.body, '', [], key);
    commandManager.assertLastCommand(null);
  });

  test('delete command triggers', function() {
    store.data.selection.items = new Set(['12', '13']);
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document.body, 46, '', 'Delete');
    commandManager.assertLastCommand('delete', ['12', '13']);
  });

  test('copy command triggers', function() {
    var modifier = cr.isMac ? 'meta' : 'ctrl';

    store.data.selection.items = new Set(['11', '13']);
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document.body, '', modifier, 'c');
    commandManager.assertLastCommand('copy', ['11', '13']);
  });

  test('cut/paste commands trigger', function() {
    var lastCut;
    var lastPaste;
    chrome.bookmarkManagerPrivate.cut = function(idList) {
      lastCut = idList.sort();
    };
    chrome.bookmarkManagerPrivate.paste = function(selectedFolder) {
      lastPaste = selectedFolder;
    };

    var modifier = cr.isMac ? 'meta' : 'ctrl';

    store.data.selection.items = new Set(['11', '13']);
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document.body, '', modifier, 'x');
    assertDeepEquals(['11', '13'], lastCut);
    MockInteractions.pressAndReleaseKeyOn(document.body, '', modifier, 'v');
    assertEquals('1', lastPaste);
  });

  test('undo and redo commands trigger', function() {
    var undoModifier = cr.isMac ? 'meta' : 'ctrl';
    var undoKey = 'z';
    var redoModifier = cr.isMac ? ['meta', 'shift'] : 'ctrl'
    var redoKey = cr.isMac ? 'Z' : 'y';

    MockInteractions.pressAndReleaseKeyOn(
        document.body, '', undoModifier, undoKey);
    commandManager.assertLastCommand('undo');

    MockInteractions.pressAndReleaseKeyOn(
        document.body, '', redoModifier, redoKey);
    commandManager.assertLastCommand('redo');
  });

  test.only('Show In Folder is only available during search', function() {
    store.data.selection.items = new Set(['12']);
    store.notifyObservers();

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.LIST);
    Polymer.dom.flush();

    var showInFolderItem =
        commandManager.root.querySelector('[command=show-in-folder]');

    // Show in folder hidden when search is inactive.
    assertTrue(showInFolderItem.hidden);

    // Show in Folder visible when search is active.
    store.data.search.term = 'test';
    store.data.search.results = ['12', '13'];
    store.notifyObservers();
    commandManager.closeCommandMenu();
    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.LIST);
    assertFalse(showInFolderItem.hidden);

    // Show in Folder hidden when menu is opened from the sidebar.
    commandManager.closeCommandMenu();
    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.TREE);
    assertTrue(showInFolderItem.hidden);

    // Show in Folder hidden when multiple items are selected.
    store.data.selection.items = new Set(['12', '13']);
    store.notifyObservers();
    commandManager.closeCommandMenu();
    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.LIST);
    assertTrue(showInFolderItem.hidden);

    // Executing the command selects the parent folder.
    commandManager.handle(Command.SHOW_IN_FOLDER, new Set(['12']));
    assertEquals('select-folder', store.lastAction.name);
    assertEquals('1', store.lastAction.id);
  });

  test('does not delete children at same time as ancestor', function() {
    var lastDelete = null;
    chrome.bookmarkManagerPrivate.removeTrees = function(idArray) {
      lastDelete = idArray.sort();
    };

    var parentAndChildren = new Set(['11', '12', '111', '1221']);
    assertTrue(commandManager.canExecute(Command.DELETE, parentAndChildren));
    commandManager.handle(Command.DELETE, parentAndChildren);

    assertDeepEquals(['11', '12'], lastDelete);
  });

  test('expandUrls_ expands one level of URLs', function() {
    var urls = commandManager.expandUrls_(new Set(['1']));
    assertDeepEquals(['http://13/'], urls);

    urls = commandManager.expandUrls_(new Set(['11', '12', '13']));
    assertDeepEquals(['http://111/', 'http://121/', 'http://13/'], urls);
  });

  test('shift-enter opens URLs in new window', function() {
    store.data.selection.items = new Set(['12', '13']);
    store.notifyObservers();

    var lastCreate;
    chrome.windows.create = function(createConfig) {
      lastCreate = createConfig;
    };

    MockInteractions.pressAndReleaseKeyOn(document.body, 13, 'shift', 'Enter');
    commandManager.assertLastCommand(Command.OPEN_NEW_WINDOW, ['12', '13']);
    assertDeepEquals(['http://121/', 'http://13/'], lastCreate.url);
    assertFalse(lastCreate.incognito);
  });

  test('shift-enter does not trigger enter commands', function() {
    // Enter by itself performs an edit (Mac) or open (non-Mac). Ensure that
    // shift-enter doesn't trigger those commands.
    store.data.selection.items = new Set(['13']);
    store.notifyObservers();

    MockInteractions.pressAndReleaseKeyOn(document.body, 13, 'shift', 'Enter');
    commandManager.assertLastCommand(Command.OPEN_NEW_WINDOW);
  });

  test('opening many items causes a confirmation dialog', function() {
    var lastCreate = null;
    chrome.windows.create = function(createConfig) {
      lastCreate = createConfig;
    };

    var items = new Set(['3']);
    assertTrue(commandManager.canExecute(Command.OPEN_NEW_WINDOW, items));

    commandManager.handle(Command.OPEN_NEW_WINDOW, items);
    // No window should be created right away.
    assertEquals(null, lastCreate);

    var dialog = commandManager.$.openDialog.getIfExists();
    assertTrue(dialog.open);

    // Pressing 'cancel' should not open the window.
    MockInteractions.tap(dialog.querySelector('.cancel-button'));
    assertFalse(dialog.open);
    assertEquals(null, lastCreate);

    commandManager.handle(Command.OPEN_NEW_WINDOW, items);
    assertTrue(dialog.open);

    // Pressing 'yes' will open all the URLs.
    MockInteractions.tap(dialog.querySelector('.action-button'));
    assertFalse(dialog.open);
    assertEquals(20, lastCreate.url.length);
  });

  test('cannot execute "Open in New Tab" on folders with no items', function() {
    var items = new Set(['2']);
    assertFalse(commandManager.canExecute(Command.OPEN_NEW_TAB, items));

    store.data.selection.items = items;

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.LIST);
    Polymer.dom.flush();

    var commandItem = {};
    commandManager.root.querySelectorAll('.dropdown-item').forEach(element => {
      commandItem[element.getAttribute('command')] = element;
    });

    assertTrue(commandItem[Command.OPEN_NEW_TAB].disabled);
    assertFalse(commandItem[Command.OPEN_NEW_TAB].hidden);

    assertTrue(commandItem[Command.OPEN_NEW_WINDOW].disabled);
    assertFalse(commandItem[Command.OPEN_NEW_WINDOW].hidden);

    assertTrue(commandItem[Command.OPEN_INCOGNITO].disabled);
    assertFalse(commandItem[Command.OPEN_INCOGNITO].hidden);
  });

  test('cannot execute editing commands when editing is disabled', function() {
    var items = new Set(['12']);

    store.data.prefs.canEdit = false;
    store.data.selection.items = items;
    store.notifyObservers();

    assertFalse(commandManager.canExecute(Command.EDIT, items));
    assertFalse(commandManager.canExecute(Command.DELETE, items));
    assertFalse(commandManager.canExecute(Command.UNDO, items));
    assertFalse(commandManager.canExecute(Command.REDO, items));

    // No divider line should be visible when only 'Open' commands are enabled.
    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.LIST);
    commandManager.root.querySelectorAll('hr').forEach(element => {
      assertTrue(element.hidden);
    });
  });

  test('cannot edit unmodifiable nodes', function() {
    // Cannot edit root folders.
    var items = new Set(['1']);
    store.data.selection.items = items;
    assertFalse(commandManager.canExecute(Command.EDIT, items));
    assertFalse(commandManager.canExecute(Command.DELETE, items));

    store.data.nodes['12'].unmodifiable = 'managed';
    store.notifyObservers();

    items = new Set(['12']);
    assertFalse(commandManager.canExecute(Command.EDIT, items));
    assertFalse(commandManager.canExecute(Command.DELETE, items));

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.LIST);
    var commandItem = {};
    commandManager.root.querySelectorAll('.dropdown-item').forEach(element => {
      commandItem[element.getAttribute('command')] = element;
    });
    MockInteractions.tap(commandItem[Command.EDIT]);
    commandManager.assertLastCommand(null);
  });
});

suite('<bookmarks-item> CommandManager integration', function() {
  var list;
  var items;
  var commandManager;
  var openedTabs;

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(createFolder(
          '1',
          [
            createFolder(
                '11',
                [
                  createItem('111', {url: 'http://111/'}),
                ]),
            createItem('12', {url: 'http://12/'}),
            createItem('13', {url: 'http://13/'}),
          ])),
      selectedFolder: '1',
    });
    store.setReducersEnabled(true);
    store.replaceSingleton();

    commandManager = document.createElement('bookmarks-command-manager');

    list = document.createElement('bookmarks-list');
    replaceBody(list);
    document.body.appendChild(commandManager);
    Polymer.dom.flush();

    items = list.root.querySelectorAll('bookmarks-item');

    openedTabs = [];
    chrome.tabs.create = function(createConfig) {
      openedTabs.push(createConfig);
    }
  });

  function assertOpenedTabs(tabs) {
    assertDeepEquals(tabs, openedTabs.map(createConfig => createConfig.url));
  }

  function simulateDoubleClick(element, config) {
    config = config || {};
    customClick(element, config);
    config.detail = 2;
    customClick(element, config);
  }

  test('double click opens folders in bookmark manager', function() {
    simulateDoubleClick(items[0]);
    assertEquals(store.data.selectedFolder, '11');
  });

  test('double click opens items in foreground tab', function() {
    simulateDoubleClick(items[1]);
    assertOpenedTabs(['http://12/']);
  });

  test('shift-double click opens full selection', function() {
    // Shift-double click works because the first click event selects the range
    // of items, then the second doubleclick event opens that whole selection.
    customClick(items[0]);
    simulateDoubleClick(items[1], {shiftKey: true});

    assertOpenedTabs(['http://111/', 'http://12/']);
  });

  test('control-double click opens full selection', function() {
    customClick(items[0]);
    simulateDoubleClick(items[2], {ctrlKey: true});

    assertOpenedTabs(['http://111/', 'http://13/']);
  });
});
