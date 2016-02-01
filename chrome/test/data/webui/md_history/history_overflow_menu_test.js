// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_overflow_menu_test', function() {
  // Menu button event.
  var MENU_EVENT = {
    detail: {
      accessTime: 1
    }
  };

  var ADDITIONAL_MENU_EVENT = {
    detail: {
      accessTime: 2
    }
  };

  function registerTests() {
    suite('#overflow-menu', function() {
      var element;

      suiteSetup(function() {
        element = $('history-card-manager');
      });

      test('opening and closing menu', function() {
        element.toggleMenu_(MENU_EVENT);
        assertEquals(true, element.menuOpen);
        assertEquals(1, element.menuIdentifier);

        // Test having the same menu event (pressing the same button) closes the
        // overflow menu.
        element.toggleMenu_(MENU_EVENT);
        assertEquals(false, element.menuOpen);

        // Test having consecutive distinct menu events moves the menu to the
        // new button.
        element.toggleMenu_(MENU_EVENT);
        element.toggleMenu_(ADDITIONAL_MENU_EVENT);
        assertEquals(2, element.menuIdentifier);
        assertEquals(true, element.menuOpen);
        element.toggleMenu_(MENU_EVENT);
        assertEquals(true, element.menuOpen);
        assertEquals(1, element.menuIdentifier);

        element.closeMenu();
        assertEquals(false, element.menuOpen);
        assertEquals(1, element.menuIdentifier);
      });

      test('keyboard input for closing menu', function() {
        // Test that pressing escape on the document closes the menu. In the
        // actual page, it will take two presses to close the menu due to focus.
        // TODO(yingran): Fix this behavior to only require one key press.
        element.toggleMenu_(MENU_EVENT);
        MockInteractions.pressAndReleaseKeyOn(document, 27);
        assertEquals(false, element.menuOpen);
      });

      teardown(function() {
        element.menuIdentifier = 0;
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
