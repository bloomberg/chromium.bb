// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_overflow_menu_test', function() {
  // Menu button event.
  var MENU_EVENT = {
    detail: {
      target: null
    }
  };

  var ADDITIONAL_MENU_EVENT = {
    detail: {
      target: null
    }
  };

  function registerTests() {
    suite('#overflow-menu', function() {
      var element;

      suiteSetup(function() {
        element = $('history-app').$['history-list'];

        var element1 = document.createElement('div');
        var element2 = document.createElement('div');
        document.body.appendChild(element1);
        document.body.appendChild(element2);

        MENU_EVENT.detail.target = element1;
        ADDITIONAL_MENU_EVENT.detail.target = element2;
      });

      test('opening and closing menu', function() {
        element.toggleMenu_(MENU_EVENT);
        assertEquals(true, element.$.sharedMenu.menuOpen);
        assertEquals(MENU_EVENT.detail.target,
                     element.$.sharedMenu.lastAnchor_);

        // Test having the same menu event (pressing the same button) closes the
        // overflow menu.
        element.toggleMenu_(MENU_EVENT);
        assertEquals(false, element.$.sharedMenu.menuOpen);

        // Test having consecutive distinct menu events moves the menu to the
        // new button.
        element.toggleMenu_(MENU_EVENT);
        element.toggleMenu_(ADDITIONAL_MENU_EVENT);
        assertEquals(ADDITIONAL_MENU_EVENT.detail.target,
                     element.$.sharedMenu.lastAnchor_);
        assertEquals(true, element.$.sharedMenu.menuOpen);
        element.toggleMenu_(MENU_EVENT);
        assertEquals(true, element.$.sharedMenu.menuOpen);
        assertEquals(MENU_EVENT.detail.target,
                     element.$.sharedMenu.lastAnchor_);

        element.$.sharedMenu.closeMenu();
        assertEquals(false, element.$.sharedMenu.menuOpen);
        assertEquals(MENU_EVENT.detail.target,
                     element.$.sharedMenu.lastAnchor_);
      });

      test('keyboard input for closing menu', function() {
        // Test that pressing escape on the document closes the menu. In the
        // actual page, it will take two presses to close the menu due to focus.
        // TODO(yingran): Fix this behavior to only require one key press.
        element.toggleMenu_(MENU_EVENT);
        MockInteractions.pressAndReleaseKeyOn(document, 27);
        assertEquals(false, element.$.sharedMenu.menuOpen);
      });

      teardown(function() {
        element.$.sharedMenu.lastAnchor_ = null;
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
