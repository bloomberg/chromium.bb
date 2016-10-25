// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for settings-action-menu element. */
suite('SettingsActionMenu', function() {
  /** @type {?SettingsActionMenuElement} */
  var menu = null;

  /** @type {?NodeList<HTMLElement>} */
  var items = null;

  setup(function() {
    PolymerTest.clearBody();

    document.body.innerHTML = `
      <button id="dots">...</button>
      <dialog is="settings-action-menu">
        <button class="dropdown-item">Un</button>
        <hr>
        <button class="dropdown-item">Dos</button>
        <button class="dropdown-item">Tres</button>
      </dialog>
    `;

    menu = document.querySelector('dialog[is=settings-action-menu]');
    items = menu.querySelectorAll('.dropdown-item');
    assertEquals(3, items.length);
  });

  teardown(function() {
    if (menu.open)
      menu.close();
  });

  test('focus after showing', function() {
    menu.showAt(document.querySelector('#dots'));
    assertEquals(menu.root.activeElement, items[0]);

    menu.close();
    items[0].hidden = true;
    menu.showAt(document.querySelector('#dots'));
    assertEquals(menu.root.activeElement, items[1]);

    menu.close();
    items[1].hidden = true;
    menu.showAt(document.querySelector('#dots'));
    assertEquals(menu.root.activeElement, items[2]);

    menu.close();
    items[2].disabled = true;
    menu.showAt(document.querySelector('#dots'));
    assertEquals(null, menu.root.activeElement);
  });

  test('focus after down/up arrow', function() {
    function down() {
      MockInteractions.keyDownOn(menu, 'ArrowDown', [], 'ArrowDown');
    }

    function up() {
      MockInteractions.keyDownOn(menu, 'ArrowUp', [], 'ArrowUp');
    }

    menu.showAt(document.querySelector('#dots'));
    assertEquals(items[0], menu.root.activeElement);

    down();
    assertEquals(items[1], menu.root.activeElement);
    down();
    assertEquals(items[2], menu.root.activeElement);
    down();
    assertEquals(items[0], menu.root.activeElement);
    up();
    assertEquals(items[2], menu.root.activeElement);
    up();
    assertEquals(items[1], menu.root.activeElement);
    up();
    assertEquals(items[0], menu.root.activeElement);
    up();
    assertEquals(items[2], menu.root.activeElement);

    items[1].disabled = true;
    up();
    assertEquals(items[0], menu.root.activeElement);
  });

  test('close on resize', function() {
    menu.showAt(document.querySelector('#dots'));
    assertTrue(menu.open);

    window.dispatchEvent(new CustomEvent('resize'));
    assertFalse(menu.open);
  });

  test('close on Tab', function() {
    menu.showAt(document.querySelector('#dots'));
    assertTrue(menu.open);

    MockInteractions.keyDownOn(menu, 'Tab', [], 'Tab');
    assertFalse(menu.open);
  });
});
