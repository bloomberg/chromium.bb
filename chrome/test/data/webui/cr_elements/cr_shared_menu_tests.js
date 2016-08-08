// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-shared-menu. */
suite('cr-shared-menu', function() {
  var menu;

  var button;
  var button2;

  var item1;
  var item2;
  var item3;

  function afterOpen(callback) {
    menu.addEventListener('iron-overlay-opened', function f() {
      menu.removeEventListener('iron-overlay-opened', f);
      callback();
    });
  }

  function afterClose(callback) {
    menu.addEventListener('iron-overlay-closed', function f() {
      menu.removeEventListener('iron-overlay-closed', f);
      callback();
    });
  }

  suiteSetup(function() {
    return PolymerTest.importHtml(
        'chrome://resources/polymer/v1_0/paper-item/paper-item.html');
  });

  setup(function() {
    PolymerTest.clearBody();
    // Basic wiring to set up a menu which opens when a button is pressed.
    menu = document.createElement('cr-shared-menu');
    menu.$$('#dropdown').noAnimations = true;

    item1 = document.createElement('paper-item');
    menu.appendChild(item1);
    item2 = document.createElement('paper-item');
    menu.appendChild(item2);
    item3 = document.createElement('paper-item');
    menu.appendChild(item3);

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
      assertEquals(item1, menu.shadowRoot.activeElement);

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

  test('focus is trapped inside the menu', function(done) {
    button.focus();
    MockInteractions.tap(button);

    afterOpen(function() {
      // Simulate shift-tab on first element.
      assertEquals(item1, menu.shadowRoot.activeElement);
      MockInteractions.pressAndReleaseKeyOn(item1, 9, ['shift']);
      assertEquals(item3, menu.shadowRoot.activeElement);

      // Simulate tab on last element.
      MockInteractions.pressAndReleaseKeyOn(item3, 9);
      assertEquals(item1, menu.shadowRoot.activeElement);

      done();
    });
  });
});
