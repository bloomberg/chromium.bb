// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-shared-menu. */
suite('cr-shared-menu', function() {
  var menu;

  var button;
  var button2;

  var items = [];

  function afterOpen(callback) {
    listenOnce(menu, 'iron-overlay-opened', function () {
      // paper-listbox applies focus after opening with microtask timing.
      // Delay until that has happened:
      setTimeout(callback, 0);
    });
  }

  function afterClose(callback) {
    listenOnce(menu, 'iron-overlay-closed', () => callback());
  }

  suiteSetup(function() {
    return PolymerTest.importHtml(
        'chrome://resources/polymer/v1_0/paper-item/paper-item.html');
  });

  setup(function() {
    PolymerTest.clearBody();
    // Basic wiring to set up an empty menu that opens when a button is pressed.
    menu = document.createElement('cr-shared-menu');
    menu.$$('#dropdown').noAnimations = true;

    button = document.createElement('button');
    button.addEventListener('tap', function(e) {
      menu.toggleMenu(button, {});
      e.stopPropagation();  // Prevent 'tap' from closing the dialog.
    });

    button2 = document.createElement('button');
    button2.addEventListener('tap', function(e) {
      menu.toggleMenu(button2, {});
      e.stopPropagation();  // Prevent 'tap' from closing the dialog.
    });

    document.body.appendChild(menu);
    document.body.appendChild(button);
    document.body.appendChild(button2);
  });

  suite('basic', function() {
    setup(function() {
      // Populate the menu with paper-items.
      for (var i = 0; i < 3; i++) {
        items[i] = document.createElement('paper-item');
        menu.appendChild(items[i]);
      }
    });

    test('opening and closing menu', function(done) {
      MockInteractions.tap(button);
      assertTrue(menu.menuOpen);

      afterOpen(function() {
        // Using tap to close the menu requires that the iron-overlay-behavior
        // has finished initializing, which happens asynchronously between
        // tapping the button and firing iron-overlay-opened.
        MockInteractions.tap(document.body);
        assertFalse(menu.menuOpen);

        done();
      });
    });

    test('open and close menu with escape', function(done) {
      MockInteractions.tap(button);
      assertTrue(menu.menuOpen);
      afterOpen(function() {
        // Pressing escape should close the menu.
        MockInteractions.pressAndReleaseKeyOn(menu, 27);
        assertFalse(menu.menuOpen);
        done();
      });
    });

    test('refocus button on close', function(done) {
      button.focus();
      MockInteractions.tap(button);

      afterOpen(function() {
        assertTrue(menu.menuOpen);
        // Focus is applied asynchronously after the menu is opened.
        assertEquals(items[0], menu.shadowRoot.activeElement);

        menu.closeMenu();

        afterClose(function() {
          assertFalse(menu.menuOpen);
          // Button should regain focus after closing the menu.
          assertEquals(button, document.activeElement);
          done();
        });

      });
    });

    test('refocus latest button on close', function(done) {
      // Regression test for crbug.com/628080.
      button.focus();
      MockInteractions.tap(button);

      afterOpen(function() {
        button2.focus();
        MockInteractions.tap(button2);

        afterOpen(function() {
          menu.closeMenu();
          afterClose(function() {
            assertEquals(button2, document.activeElement);
            done();
          });
        });
      });
    });

    test('closeMenu does not refocus button when focus moves', function(done) {
      var input = document.createElement('input');
      document.body.appendChild(input);

      button.focus();
      MockInteractions.tap(button);

      afterOpen(function() {
        input.focus();
        menu.closeMenu();

        afterClose(function() {
          assertEquals(input, document.activeElement);
          done();
        });
      });
    });

    test('tab closes menu', function(done) {
      button.focus();
      MockInteractions.tap(button);

      afterOpen(function() {
        MockInteractions.pressAndReleaseKeyOn(items[0], 9);
        afterClose(function() {
          // Focus should move to a different element, but we can't simulate
          // the right events to test this.
          done();
        });
      });
    });

    test('shift-tab closes menu', function(done) {
      button.focus();
      MockInteractions.tap(button);

      afterOpen(function() {
        MockInteractions.pressAndReleaseKeyOn(items[0], 9, ['shift']);
        afterClose(done);
      });
    });

    test('up and down change focus', function(done) {
      button.focus();
      MockInteractions.tap(button);

      afterOpen(function() {
        // Pressing down on first item goes to second item.
        assertEquals(items[0], document.activeElement);
        MockInteractions.pressAndReleaseKeyOn(items[0], 40);
        assertEquals(items[1], document.activeElement);

        // Pressing down twice more cycles back to first item.
        MockInteractions.pressAndReleaseKeyOn(items[1], 40);
        MockInteractions.pressAndReleaseKeyOn(items[2], 40);
        assertEquals(items[0], document.activeElement);

        // Pressing up cycles to last item.
        MockInteractions.pressAndReleaseKeyOn(items[0], 38);
        assertEquals(items[2], document.activeElement);

        done();
      });
    });
  });

  suite('different item types', function() {
    setup(function() {
      // Populate the menu with tabbable and untabbable items.
      items[0] = document.createElement('paper-item');
      items[0].disabled = true;
      menu.appendChild(items[0]);

      items[1] = document.createElement('paper-item');
      menu.appendChild(items[1]);

      items[2] = document.createElement('button');
      menu.appendChild(items[2]);

      items[3] = document.createElement('button');
      items[3].disabled = true;
      menu.appendChild(items[3]);

      items[4] = document.createElement('paper-item');
      items[4].hidden = true;
      menu.appendChild(items[4]);
    });

    test('focus does not start/end with untabbable elements', function(done) {
      button.focus();
      MockInteractions.tap(button);

      afterOpen(function() {
        // The first item is disabled, so the second item is the first tabbable
        // item and should be focusced.
        assertEquals(items[1], menu.shadowRoot.activeElement);

        // The last two items are disabled or hidden, so they should be skipped
        // when pressing up.
        MockInteractions.pressAndReleaseKeyOn(items[1], 38);
        assertEquals(items[2], menu.shadowRoot.activeElement);

        // Simulate pressing down on last focusable element to wrap to first
        // focusable element.
        MockInteractions.pressAndReleaseKeyOn(items[2], 40);
        assertEquals(items[1], menu.shadowRoot.activeElement);

        done();
      });
    });
  });
});
