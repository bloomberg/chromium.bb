// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-keyboard-shortcuts. */
cr.define('extension_keyboard_shortcut_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Layout: 'Layout',
    // The ShortcutUtil test suite is all js-based (no UI), so we can execute
    // multiple in a single browser test without worrying about timing out.
    ShortcutUtil: 'ExtensionShortcutUtilTest',
  };

  function registerTests() {
    suite('ExtensionKeyboardShortcutTest', function() {
      /** @type {extensions.KeyboardShortcuts} */
      var keyboardShortcuts;
      /** @type {chrome.developerPrivate.ExtensionInfo} */
      var noCommand;
      /** @type {chrome.developerPrivate.ExtensionInfo} */
      var oneCommand;
      /** @type {chrome.developerPrivate.ExtensionInfo} */
      var twoCommands;

      setup(function() {
        PolymerTest.clearBody();
        keyboardShortcuts = new extensions.KeyboardShortcuts();

        var createInfo = extension_test_util.createExtensionInfo;
        noCommands = createInfo({id: 'a'.repeat(32)});
        oneCommand = createInfo({
          id: 'b'.repeat(32),
          commands: [{
            description: 'Description',
            keybinding: 'Ctrl+W',
            name: 'bCommand',
            isActive: true,
            scope: 'CHROME',
            isExtensionAction: true,
          }]
        });
        twoCommands = createInfo({
          id: 'c'.repeat(32),
          commands: [{
            description: 'Another Description',
            keybinding: 'Alt+F4',
            name: 'cCommand',
            isActive: true,
            scope: 'GLOBAL',
            isExtensionAction: false,
          }, {
            description: 'Yet Another Description',
            keybinding: '',
            name: 'cCommand2',
            isActive: false,
            scope: 'CHROME',
            isExtensionAction: false,
          }]
        });

        keyboardShortcuts.set('items', [noCommands, oneCommand, twoCommands]);

        document.body.appendChild(keyboardShortcuts);

        Polymer.dom.flush();
      });

      test(assert(TestNames.Layout), function() {
        var isVisibleOnCard = function(e, s) {
          // We check the light DOM in the card because it's a regular old div,
          // rather than a fancy-schmancy custom element.
          return extension_test_util.isVisible(e, s, true);
        };
        var cards =
            keyboardShortcuts.$$('#main').querySelectorAll('.shortcut-card');
        assertEquals(2, cards.length);

        var card1 = cards[0];
        expectEquals(oneCommand.name,
                     card1.querySelector('.card-title span').textContent);
        var commands = card1.querySelectorAll('.command-entry');
        assertEquals(1, commands.length);
        expectTrue(isVisibleOnCard(commands[0], '.command-name'));
        expectTrue(isVisibleOnCard(commands[0], 'select.md-select'));

        var card2 = cards[1];
        commands = card2.querySelectorAll('.command-entry');
        assertEquals(2, commands.length);
      });
    });
    suite(assert(TestNames.ShortcutUtil), function() {
      test('isValidKeyCode test', function() {
        expectTrue(extensions.isValidKeyCode('A'.charCodeAt(0)));
        expectTrue(extensions.isValidKeyCode('F'.charCodeAt(0)));
        expectTrue(extensions.isValidKeyCode('Z'.charCodeAt(0)));
        expectTrue(extensions.isValidKeyCode('4'.charCodeAt(0)));
        expectTrue(extensions.isValidKeyCode(extensions.Key.PageUp));
        expectTrue(extensions.isValidKeyCode(extensions.Key.MediaPlayPause));
        expectTrue(extensions.isValidKeyCode(extensions.Key.Down));
        expectFalse(extensions.isValidKeyCode(16));  // Shift
        expectFalse(extensions.isValidKeyCode(17));  // Ctrl
        expectFalse(extensions.isValidKeyCode(18));  // Alt
        expectFalse(extensions.isValidKeyCode(113));  // F2
        expectFalse(extensions.isValidKeyCode(144));  // Num Lock
        expectFalse(extensions.isValidKeyCode(43));  // +
        expectFalse(extensions.isValidKeyCode(27));  // Escape
      });

      test('keystrokeToString test', function() {
        // Creating an event with the KeyboardEvent ctor doesn't work. Fake it.
        var e = {keyCode: 'A'.charCodeAt(0)};
        expectEquals('A', extensions.keystrokeToString(e));
        e.ctrlKey = true;
        expectEquals('Ctrl+A', extensions.keystrokeToString(e));
        e.shiftKey = true;
        expectEquals('Ctrl+Shift+A', extensions.keystrokeToString(e));
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
