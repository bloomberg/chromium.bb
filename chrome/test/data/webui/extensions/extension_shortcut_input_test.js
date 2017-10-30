// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-keyboard-shortcuts. */
cr.define('extension_shortcut_input_tests', function() {
  class DelegateMock extends TestBrowserProxy {
    constructor() {
      super(['setShortcutHandlingSuspended', 'updateExtensionCommand']);
    }

    /** @param {boolean} enable */
    setShortcutHandlingSuspended(enable) {
      this.methodCalled('setShortcutHandlingSuspended', enable);
    }

    /**
     * @param {string} item
     * @param {string} commandName
     * @param {string} keybinding
     */
    updateExtensionCommand(item, commandName, keybinding) {
      this.methodCalled(
          'updateExtensionCommand', [item, commandName, keybinding]);
    }
  }

  /** @enum {string} */
  var TestNames = {
    Basic: 'basic',
  };

  suite('ExtensionShortcutInputTest', function() {
    /** @type {extensions.ShortcutInput} */
    var input;
    setup(function() {
      PolymerTest.clearBody();
      input = new extensions.ShortcutInput();
      input.delegate = new DelegateMock();
      input.commandName = 'Command';
      input.item = 'itemid';
      document.body.appendChild(input);
      Polymer.dom.flush();
    });

    test(assert(TestNames.Basic), function() {
      var field = input.$['input'];
      var fieldText = function() {
        return field.value;
      };
      expectEquals('', fieldText());
      var isClearVisible =
          extension_test_util.isVisible.bind(null, input, '#clear', false);
      expectFalse(isClearVisible());

      // Click the input. Capture should start.
      MockInteractions.tap(field);
      return input.delegate.whenCalled('setShortcutHandlingSuspended')
          .then((arg) => {
            assertTrue(arg);
            input.delegate.reset();

            expectEquals('', fieldText());
            expectFalse(isClearVisible());

            // Press ctrl.
            MockInteractions.keyDownOn(field, 17, ['ctrl']);
            expectEquals('Ctrl', fieldText());
            // Add shift.
            MockInteractions.keyDownOn(field, 16, ['ctrl', 'shift']);
            expectEquals('Ctrl + Shift', fieldText());
            // Remove shift.
            MockInteractions.keyUpOn(field, 16, ['ctrl']);
            expectEquals('Ctrl', fieldText());
            // Add alt (ctrl + alt is invalid).
            MockInteractions.keyDownOn(field, 18, ['ctrl', 'alt']);
            expectEquals('invalid', fieldText());
            // Remove alt.
            MockInteractions.keyUpOn(field, 18, ['ctrl']);
            expectEquals('Ctrl', fieldText());

            // Add 'A'. Once a valid shortcut is typed (like Ctrl + A), it is
            // committed.
            MockInteractions.keyDownOn(field, 65, ['ctrl']);
            return input.delegate.whenCalled('updateExtensionCommand');
          })
          .then((arg) => {
            input.delegate.reset();
            expectDeepEquals(['itemid', 'Command', 'Ctrl+A'], arg);
            expectEquals('Ctrl + A', fieldText());
            expectEquals('Ctrl+A', input.shortcut);
            expectTrue(isClearVisible());

            // Test clearing the shortcut.
            MockInteractions.tap(input.$['clear']);
            return input.delegate.whenCalled('updateExtensionCommand');
          })
          .then((arg) => {
            input.delegate.reset();
            expectDeepEquals(['itemid', 'Command', ''], arg);
            expectEquals('', input.shortcut);
            expectFalse(isClearVisible());

            MockInteractions.tap(field);
            return input.delegate.whenCalled('setShortcutHandlingSuspended');
          })
          .then((arg) => {
            input.delegate.reset();
            expectTrue(arg);

            // Test ending capture using the escape key.
            MockInteractions.keyDownOn(field, 27);  // Escape key.
            return input.delegate.whenCalled('setShortcutHandlingSuspended');
          })
          .then(expectFalse);
    });
  });

  return {
    TestNames: TestNames,
  };
});
