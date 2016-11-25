// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

suite('#overflow-menu', function() {
  var app;
  var listContainer;
  var sharedMenu;

  suiteSetup(function() {
    app = $('history-app');
    listContainer = app.$['history'];
    var element1 = document.createElement('div');
    var element2 = document.createElement('div');
    document.body.appendChild(element1);
    document.body.appendChild(element2);

    MENU_EVENT.detail.target = element1;
    ADDITIONAL_MENU_EVENT.detail.target = element2;
    sharedMenu = listContainer.$.sharedMenu.get();
  });

  test('opening and closing menu', function() {
    listContainer.toggleMenu_(MENU_EVENT);
    assertTrue(sharedMenu.menuOpen);
    assertEquals(MENU_EVENT.detail.target, sharedMenu.lastAnchor_);

    // Test having the same menu event (pressing the same button) closes
    // the overflow menu.
    listContainer.toggleMenu_(MENU_EVENT);
    assertFalse(sharedMenu.menuOpen);

    // Test having consecutive distinct menu events moves the menu to the
    // new button.
    listContainer.toggleMenu_(MENU_EVENT);
    listContainer.toggleMenu_(ADDITIONAL_MENU_EVENT);
    assertEquals(ADDITIONAL_MENU_EVENT.detail.target, sharedMenu.lastAnchor_);
    assertTrue(sharedMenu.menuOpen);

    listContainer.toggleMenu_(MENU_EVENT);
    assertTrue(sharedMenu.menuOpen);
    assertEquals(MENU_EVENT.detail.target, sharedMenu.lastAnchor_);

    sharedMenu.closeMenu();
    assertFalse(sharedMenu.menuOpen);
  });

  test('menu closes when search changes', function() {
    var entry =
        [createHistoryEntry('2016-07-19', 'https://www.nianticlabs.com')];

    app.historyResult(createHistoryInfo(), entry);
    listContainer.toggleMenu_(MENU_EVENT);
    // Menu closes when search changes.
    app.historyResult(createHistoryInfo('niantic'), entry);
    assertFalse(sharedMenu.menuOpen);
  });

  teardown(function() {
    sharedMenu.lastAnchor_ = null;
  });
});
