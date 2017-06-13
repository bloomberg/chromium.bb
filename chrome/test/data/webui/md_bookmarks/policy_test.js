// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Bookmarks policies', function() {
  var store;
  var app;

  setup(function() {
    store = new bookmarks.TestStore({
      nodes: testTree(createFolder(
          '1',
          [
            createItem('11'),
          ])),
      selectedFolder: '1',
    });
    store.setReducersEnabled(true);
    store.expectAction('set-incognito-availability');
    store.expectAction('set-can-edit');
    store.replaceSingleton();

    app = document.createElement('bookmarks-app');
    replaceBody(app);
  });

  test('incognito availability updates when changed', function() {
    var commandManager = bookmarks.CommandManager.getInstance();
    // Incognito is disabled during testGenPreamble(). Wait for the front-end to
    // load the config.
    return store.waitForAction('set-incognito-availability').then(action => {
      assertEquals(IncognitoAvailability.DISABLED,
          store.data.prefs.incognitoAvailability);
      assertFalse(
          commandManager.canExecute(Command.OPEN_INCOGNITO, new Set(['11'])));

      return cr.sendWithPromise(
          'testSetIncognito', IncognitoAvailability.ENABLED);
    }).then(() => {
      assertEquals(IncognitoAvailability.ENABLED,
          store.data.prefs.incognitoAvailability);
      assertTrue(
          commandManager.canExecute(Command.OPEN_INCOGNITO, new Set(['11'])));
    });
  });

  test('canEdit updates when changed', function() {
    var commandManager = bookmarks.CommandManager.getInstance();
    return store.waitForAction('set-can-edit').then(action => {
      assertFalse(store.data.prefs.canEdit);
      assertFalse(commandManager.canExecute(Command.DELETE, new Set(['11'])));

      return cr.sendWithPromise('testSetCanEdit', true);
    }).then(() => {
      assertTrue(store.data.prefs.canEdit);
      assertTrue(commandManager.canExecute(Command.DELETE, new Set(['11'])));
    });
  });
});
