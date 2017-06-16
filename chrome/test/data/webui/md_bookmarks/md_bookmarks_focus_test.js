// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for MD Bookmarks which are run as interactive ui tests.
 * Should be used for tests which care about focus.
 */

var ROOT_PATH = '../../../../../';

GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_interactive_ui_test.js']);
GEN('#include "base/command_line.h"');

function MaterialBookmarksFocusTest() {}

MaterialBookmarksFocusTest.prototype = {
  __proto__: PolymerInteractiveUITest.prototype,

  browsePreload: 'chrome://bookmarks',

  commandLineSwitches:
      [{switchName: 'enable-features', switchValue: 'MaterialDesignBookmarks'}],

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'test_command_manager.js',
    'test_store.js',
    'test_util.js',
  ]),
};

TEST_F('MaterialBookmarksFocusTest', 'All', function() {
  suite('<bookmarks-folder-node>', function() {
    var rootNode;
    var store;

    function getFolderNode(id) {
      return findFolderNode(rootNode, id);
    }

    function keydown(id, key) {
      MockInteractions.keyDownOn(getFolderNode(id).$.container, '', [], key);
    }

    setup(function() {
      store = new bookmarks.TestStore({
        nodes: testTree(
            createFolder(
                '1',
                [
                  createFolder(
                      '2',
                      [
                        createFolder('3', []),
                        createFolder('4', []),
                      ]),
                  createItem('5'),
                ]),
            createFolder('7', [])),
        selectedFolder: '1',
      });
      store.setReducersEnabled(true);
      store.replaceSingleton();

      rootNode = document.createElement('bookmarks-folder-node');
      rootNode.itemId = '0';
      rootNode.depth = -1;
      replaceBody(rootNode);
      Polymer.dom.flush();
    });

    test('keyboard selection', function() {
      function assertFocused(oldFocus, newFocus) {
        assertEquals(
            '-1', getFolderNode(oldFocus).$.container.getAttribute('tabindex'));
        assertEquals(
            '0', getFolderNode(newFocus).$.container.getAttribute('tabindex'));
        assertEquals(
            getFolderNode(newFocus).$.container,
            getFolderNode(newFocus).root.activeElement);
      }

      store.data.closedFolders = new Set('2');
      store.notifyObservers();

      // The selected folder is focus enabled on attach.
      assertEquals(
          '0', getFolderNode('1').$.container.getAttribute('tabindex'));

      // Only the selected folder should be keyboard focusable.
      assertEquals(
          '-1', getFolderNode('2').$.container.getAttribute('tabindex'));

      // Give keyboard focus to the first item.
      getFolderNode('1').$.container.focus();

      // Move down into child.
      keydown('1', 'ArrowDown');

      assertDeepEquals(bookmarks.actions.selectFolder('2'), store.lastAction);
      store.data.selectedFolder = '2';
      store.notifyObservers();

      assertEquals(
          '-1', getFolderNode('1').$.container.getAttribute('tabindex'));
      assertEquals(
          '0', getFolderNode('2').$.container.getAttribute('tabindex'));
      assertFocused('1', '2');

      // Move down past closed folders.
      keydown('2', 'ArrowDown');
      assertDeepEquals(bookmarks.actions.selectFolder('7'), store.lastAction);
      assertFocused('2', '7');

      // Move down past end of list.
      store.resetLastAction();
      keydown('7', 'ArrowDown');
      assertDeepEquals(null, store.lastAction);

      // Move up past closed folders.
      keydown('7', 'ArrowUp');
      assertDeepEquals(bookmarks.actions.selectFolder('2'), store.lastAction);
      assertFocused('7', '2');

      // Move up into parent.
      keydown('2', 'ArrowUp');
      assertDeepEquals(bookmarks.actions.selectFolder('1'), store.lastAction);
      assertFocused('2', '1');

      // Move up past start of list.
      store.resetLastAction();
      keydown('1', 'ArrowUp');
      assertDeepEquals(null, store.lastAction);
    });

    test('keyboard left/right', function() {
      store.data.closedFolders = new Set('2');
      store.notifyObservers();

      // Give keyboard focus to the first item.
      getFolderNode('1').$.container.focus();

      // Pressing right descends into first child.
      keydown('1', 'ArrowRight');
      assertDeepEquals(bookmarks.actions.selectFolder('2'), store.lastAction);

      // Pressing right on a closed folder opens that folder
      keydown('2', 'ArrowRight');
      assertDeepEquals(
          bookmarks.actions.changeFolderOpen('2', true), store.lastAction);

      // Pressing right again descends into first child.
      keydown('2', 'ArrowRight');
      assertDeepEquals(bookmarks.actions.selectFolder('3'), store.lastAction);

      // Pressing right on a folder with no children does nothing.
      store.resetLastAction();
      keydown('3', 'ArrowRight');
      assertDeepEquals(null, store.lastAction);

      // Pressing left on a folder with no children ascends to parent.
      keydown('3', 'ArrowDown');
      keydown('4', 'ArrowLeft');
      assertDeepEquals(bookmarks.actions.selectFolder('2'), store.lastAction);

      // Pressing left again closes the parent.
      keydown('2', 'ArrowLeft');
      assertDeepEquals(
          bookmarks.actions.changeFolderOpen('2', false), store.lastAction);

      // RTL flips left and right.
      document.body.style.direction = 'rtl';
      keydown('2', 'ArrowLeft');
      assertDeepEquals(
          bookmarks.actions.changeFolderOpen('2', true), store.lastAction);

      keydown('2', 'ArrowRight');
      assertDeepEquals(
          bookmarks.actions.changeFolderOpen('2', false), store.lastAction);

      document.body.style.direction = 'ltr';
    });

    test('keyboard commands are passed to command manager', function() {
      var commandManager = new TestCommandManager();
      document.body.appendChild(commandManager);
      chrome.bookmarkManagerPrivate.removeTrees = function() {};

      store.data.selection.items = new Set(['3', '4']);
      store.data.selectedFolder = '2';
      store.notifyObservers();

      getFolderNode('2').$.container.focus();
      keydown('2', 'Delete');

      commandManager.assertLastCommand(Command.DELETE, ['2']);
    });
  });

  suite('<bookmarks-list>', function() {
    var list;
    var store;
    var items;
    var multiKey = cr.isMac ? 'meta' : 'ctrl';

    function keydown(item, key, modifiers) {
      MockInteractions.keyDownOn(item, '', modifiers, key);
    }

    setup(function() {
      store = new bookmarks.TestStore({
        nodes: testTree(createFolder(
            '1',
            [
              createItem('2'),
              createItem('3'),
              createItem('4'),
              createItem('5'),
              createItem('6'),
              createFolder('7', []),
            ])),
        selectedFolder: '1',
      });
      store.setReducersEnabled(true);
      store.replaceSingleton();

      list = document.createElement('bookmarks-list');
      list.style.height = '100%';
      list.style.width = '100%';
      list.style.position = 'absolute';
      replaceBody(list);
      Polymer.dom.flush();
      items = list.root.querySelectorAll('bookmarks-item');
    });

    test('simple keyboard selection', function() {
      var focusedItem = items[0];
      assertEquals('0', focusedItem.getAttribute('tabindex'));
      assertEquals(
          '0', focusedItem.$$('.more-vert-button').getAttribute('tabindex'));
      focusedItem.focus();

      keydown(focusedItem, 'ArrowDown');
      focusedItem = items[1];
      assertEquals('0', focusedItem.getAttribute('tabindex'));
      assertEquals(
          '0', focusedItem.$$('.more-vert-button').getAttribute('tabindex'));
      assertDeepEquals(['3'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'ArrowUp');
      focusedItem = items[0];
      assertEquals('0', focusedItem.getAttribute('tabindex'));
      assertDeepEquals(['2'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'End');
      focusedItem = items[5];
      assertEquals('0', focusedItem.getAttribute('tabindex'));
      assertDeepEquals(['7'], normalizeSet(store.data.selection.items));

      // Moving past the end of the list is a no-op.
      keydown(focusedItem, 'ArrowDown');
      assertEquals('0', focusedItem.getAttribute('tabindex'));
      assertDeepEquals(['7'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'Home');
      focusedItem = items[0];
      assertEquals('0', focusedItem.getAttribute('tabindex'));
      assertDeepEquals(['2'], normalizeSet(store.data.selection.items));

      // Moving past the start of the list is a no-op.
      keydown(focusedItem, 'ArrowUp');
      assertEquals('0', focusedItem.getAttribute('tabindex'));
      assertDeepEquals(['2'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'Escape');
      assertDeepEquals([], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'a', 'ctrl');
      assertDeepEquals(
          ['2', '3', '4', '5', '6', '7'],
          normalizeSet(store.data.selection.items));
    });

    test('shift selection', function() {
      var focusedItem = items[0];
      focusedItem.focus();

      keydown(focusedItem, 'ArrowDown', 'shift');
      focusedItem = items[1];
      assertDeepEquals(['2', '3'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'Escape');
      focusedItem = items[1];
      assertDeepEquals([], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'ArrowUp', 'shift');
      focusedItem = items[0];
      assertDeepEquals(['2', '3'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'ArrowDown', 'shift');
      focusedItem = items[1];
      assertDeepEquals(['3'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'ArrowDown', 'shift');
      focusedItem = items[2];
      assertDeepEquals(['3', '4'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'End', 'shift');
      focusedItem = items[2];
      assertDeepEquals(
          ['3', '4', '5', '6', '7'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'Home', 'shift');
      focusedItem = items[2];
      assertDeepEquals(['2', '3'], normalizeSet(store.data.selection.items));
    });

    test('ctrl selection', function() {
      var focusedItem = items[0];
      focusedItem.focus();

      keydown(focusedItem, ' ', 'ctrl');
      assertDeepEquals(['2'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'ArrowDown', 'ctrl');
      focusedItem = items[1];
      assertDeepEquals(['2'], normalizeSet(store.data.selection.items));
      assertEquals('3', store.data.selection.anchor);

      keydown(focusedItem, 'ArrowDown', 'ctrl');
      focusedItem = items[2];
      assertDeepEquals(['2'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, ' ', 'ctrl');
      assertDeepEquals(['2', '4'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, ' ', 'ctrl');
      assertDeepEquals(['2'], normalizeSet(store.data.selection.items));
    });

    test('ctrl+shift selection', function() {
      var focusedItem = items[0];
      focusedItem.focus();

      keydown(focusedItem, ' ', 'ctrl');
      assertDeepEquals(['2'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'ArrowDown', 'ctrl');
      focusedItem = items[1];
      assertDeepEquals(['2'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'ArrowDown', 'ctrl');
      focusedItem = items[2];
      assertDeepEquals(['2'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'ArrowDown', ['ctrl', 'shift']);
      focusedItem = items[3];
      assertDeepEquals(
          ['2', '4', '5'], normalizeSet(store.data.selection.items));

      keydown(focusedItem, 'ArrowDown', ['ctrl', 'shift']);
      focusedItem = items[3];
      assertDeepEquals(
          ['2', '4', '5', '6'], normalizeSet(store.data.selection.items));
    });

    test('keyboard commands are passed to command manager', function() {
      var commandManager = new TestCommandManager();
      document.body.appendChild(commandManager);
      chrome.bookmarkManagerPrivate.removeTrees = function() {}

      store.data.selection.items = new Set(['2', '3']);
      store.notifyObservers();

      var focusedItem = items[4];
      focusedItem.focus();

      keydown(focusedItem, 'Delete');
      // Commands should take affect on the selection, even if something else is
      // focused.
      commandManager.assertLastCommand(Command.DELETE, ['2', '3']);
    });

    test('iron-list does not steal focus on enter', function() {
      // Iron-list attempts to focus the whole <bookmarks-item> when pressing
      // enter on the menu button. This checks that we block this behavior
      // during keydown on <bookmarks-list>.
      var commandManager = new TestCommandManager();
      document.body.appendChild(commandManager);

      var button = items[0].$$('.more-vert-button');
      button.focus();
      keydown(button, 'Enter');

      assertEquals(button, items[0].root.activeElement);
    });
  });

  mocha.run();
});
