// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for cr-action-menu element. Runs as an interactive UI
 * test, since many of these tests check focus behavior.
 */
suite('CrActionMenu', function() {
  /** @type {?CrActionMenuElement} */
  var menu = null;

  /** @type {?NodeList<HTMLElement>} */
  var items = null;

  setup(function() {
    PolymerTest.clearBody();

    document.body.innerHTML = `
      <button id="dots">...</button>
      <dialog is="cr-action-menu">
        <button class="dropdown-item">Un</button>
        <hr>
        <button class="dropdown-item">Dos</button>
        <button class="dropdown-item">Tres</button>
      </dialog>
    `;

    menu = document.querySelector('dialog[is=cr-action-menu]');
    items = menu.querySelectorAll('.dropdown-item');
    assertEquals(3, items.length);
  });

  teardown(function() {
    if (menu.open)
      menu.close();
  });

  function down() {
    MockInteractions.keyDownOn(menu, 'ArrowDown', [], 'ArrowDown');
  }

  function up() {
    MockInteractions.keyDownOn(menu, 'ArrowUp', [], 'ArrowUp');
  }

  test('hidden or disabled items', function() {
    menu.showAt(document.querySelector('#dots'));
    down();
    assertEquals(menu.root.activeElement, items[0]);

    menu.close();
    items[0].hidden = true;
    menu.showAt(document.querySelector('#dots'));
    down();
    assertEquals(menu.root.activeElement, items[1]);

    menu.close();
    items[1].disabled = true;
    menu.showAt(document.querySelector('#dots'));
    down();
    assertEquals(menu.root.activeElement, items[2]);
  });

  test('focus after down/up arrow', function() {
    menu.showAt(document.querySelector('#dots'));

    // The menu should be focused when shown, but not on any of the items.
    assertEquals(menu, document.activeElement);
    assertNotEquals(items[0], menu.root.activeElement);
    assertNotEquals(items[1], menu.root.activeElement);
    assertNotEquals(items[2], menu.root.activeElement);

    down();
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

  test('pressing up arrow when no focus will focus last item', function(){
    menu.showAt(document.querySelector('#dots'));
    assertEquals(menu, document.activeElement);

    up();
    assertEquals(items[items.length - 1], menu.root.activeElement);
  });

  test('can navigate to dynamically added items', function() {
    // Can modify children after attached() and before showAt().
    var item = document.createElement('button');
    item.classList.add('dropdown-item');
    menu.insertBefore(item, items[0]);
    menu.showAt(document.querySelector('#dots'));

    down();
    assertEquals(item, menu.root.activeElement);
    down();
    assertEquals(items[0], menu.root.activeElement);

    // Can modify children while menu is open.
    menu.removeChild(item);

    up();
    // Focus should have wrapped around to final item.
    assertEquals(items[2], menu.root.activeElement);
  });

  test('close on resize', function() {
    menu.showAt(document.querySelector('#dots'));
    assertTrue(menu.open);

    window.dispatchEvent(new CustomEvent('resize'));
    assertFalse(menu.open);
  });

  test('close on popstate', function() {
    menu.showAt(document.querySelector('#dots'));
    assertTrue(menu.open);

    window.dispatchEvent(new CustomEvent('popstate'));
    assertFalse(menu.open);
  });

  /** @param {string} key The key to use for closing. */
  function testFocusAfterClosing(key) {
    return new Promise(function(resolve) {
      var dots = document.querySelector('#dots');
      menu.showAt(dots);
      assertTrue(menu.open);

      // Check that focus returns to the anchor element.
      dots.addEventListener('focus', resolve);
      MockInteractions.keyDownOn(menu, key, [], key);
      assertFalse(menu.open);
    });
  }

  test('close on Tab', function() { return testFocusAfterClosing('Tab'); });
  test('close on Escape', function() {
    return testFocusAfterClosing('Escape');
  });

  test('mouse movement focus options', function() {
    function makeMouseoverEvent(node) {
      var e = new MouseEvent('mouseover', {bubbles: true});
      node.dispatchEvent(e);
    }

    menu.showAt(document.querySelector('#dots'));

    // Moving mouse on option 1 should focus it.
    assertNotEquals(items[0], menu.root.activeElement);
    makeMouseoverEvent(items[0]);
    assertEquals(items[0], menu.root.activeElement);

    // Moving mouse on the menu (not on option) should focus the menu.
    makeMouseoverEvent(menu);
    assertNotEquals(items[0], menu.root.activeElement);
    assertEquals(menu, document.activeElement);

    // Mouse movements should override keyboard focus.
    down();
    down();
    assertEquals(items[1], menu.root.activeElement);
    makeMouseoverEvent(items[0]);
    assertEquals(items[0], menu.root.activeElement);
  });

  test('positioning', function() {
    // A 40x10 box at (100, 250).
    var config = {
      left: 100,
      top: 250,
      width: 40,
      height: 10,
      maxX: 1000,
      maxY: 2000,
    };

    // Show right and bottom aligned by default.
    menu.showAtPosition(config);
    assertTrue(menu.open);
    assertEquals('100px', menu.style.left);
    assertEquals('250px', menu.style.top);
    menu.close();

    // Center the menu horizontally.
    menu.showAtPosition(Object.assign({}, config, {
      anchorAlignmentX: AnchorAlignment.CENTER,
    }));
    var menuWidth = menu.offsetWidth;
    var menuHeight = menu.offsetHeight;
    assertEquals(`${120 - menuWidth / 2}px`, menu.style.left);
    assertEquals('250px', menu.style.top);
    menu.close();

    // Center the menu in both axes.
    menu.showAtPosition(Object.assign({}, config, {
      anchorAlignmentX: AnchorAlignment.CENTER,
      anchorAlignmentY: AnchorAlignment.CENTER,
    }));
    menuWidth = menu.offsetWidth;
    menuHeight = menu.offsetHeight;
    assertEquals(`${120 - menuWidth / 2}px`, menu.style.left);
    assertEquals(`${255 - menuHeight / 2}px`, menu.style.top);
    menu.close();

    // Left and top align the menu.
    menu.showAtPosition(Object.assign({}, config, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      anchorAlignmentY: AnchorAlignment.BEFORE_END,
    }));
    menuWidth = menu.offsetWidth;
    menuHeight = menu.offsetHeight;
    assertEquals(`${140 - menuWidth}px`, menu.style.left);
    assertEquals(`${260 - menuHeight}px`, menu.style.top);
    menu.close();

    // Being left and top aligned at (0, 0) should anchor to the bottom right.
    menu.showAtPosition(Object.assign({}, config, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      anchorAlignmentY: AnchorAlignment.BEFORE_END,
      left: 0,
      top: 0,
    }));
    menuWidth = menu.offsetWidth;
    menuHeight = menu.offsetHeight;
    assertEquals(`0px`, menu.style.left);
    assertEquals(`0px`, menu.style.top);
    menu.close();

    // Being aligned to a point in the bottom right should anchor to the top
    // left.
    menu.showAtPosition({
      left: 1000,
      top: 2000,
      maxX: 1000,
      maxY: 2000,
    });
    menuWidth = menu.offsetWidth;
    menuHeight = menu.offsetHeight;
    assertEquals(`${1000 - menuWidth}px`, menu.style.left);
    assertEquals(`${2000 - menuHeight}px`, menu.style.top);
    menu.close();

    // Alignment is reversed in RTL.
    document.body.style.direction = 'rtl';
    menu.showAtPosition(config);
    menuWidth = menu.offsetWidth;
    menuHeight = menu.offsetHeight;
    assertTrue(menu.open);
    assertEquals(140 - menuWidth, menu.offsetLeft);
    assertEquals('250px', menu.style.top);
    menu.close();
  });
});
