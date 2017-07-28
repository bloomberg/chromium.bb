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

  /** @type {HTMLElement} */
  var dots = null;

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
    dots = document.querySelector('#dots');
    assertEquals(3, items.length);
  });

  teardown(function() {
    document.body.style.direction = 'ltr';

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
    menu.showAt(dots);
    down();
    assertEquals(menu.root.activeElement, items[0]);

    menu.close();
    items[0].hidden = true;
    menu.showAt(dots);
    down();
    assertEquals(menu.root.activeElement, items[1]);

    menu.close();
    items[1].disabled = true;
    menu.showAt(dots);
    down();
    assertEquals(menu.root.activeElement, items[2]);
  });

  test('focus after down/up arrow', function() {
    menu.showAt(dots);

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

  test('pressing up arrow when no focus will focus last item', function() {
    menu.showAt(dots);
    assertEquals(menu, document.activeElement);

    up();
    assertEquals(items[items.length - 1], menu.root.activeElement);
  });

  test('can navigate to dynamically added items', function() {
    // Can modify children after attached() and before showAt().
    var item = document.createElement('button');
    item.classList.add('dropdown-item');
    menu.insertBefore(item, items[0]);
    menu.showAt(dots);

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
    menu.showAt(dots);
    assertTrue(menu.open);

    window.dispatchEvent(new CustomEvent('resize'));
    assertFalse(menu.open);
  });

  test('close on popstate', function() {
    menu.showAt(dots);
    assertTrue(menu.open);

    window.dispatchEvent(new CustomEvent('popstate'));
    assertFalse(menu.open);
  });

  /** @param {string} key The key to use for closing. */
  function testFocusAfterClosing(key) {
    return new Promise(function(resolve) {
      menu.showAt(dots);
      assertTrue(menu.open);

      // Check that focus returns to the anchor element.
      dots.addEventListener('focus', resolve);
      MockInteractions.keyDownOn(menu, key, [], key);
      assertFalse(menu.open);
    });
  }

  test('close on Tab', function() {
    return testFocusAfterClosing('Tab');
  });
  test('close on Escape', function() {
    return testFocusAfterClosing('Escape');
  });

  test('mouse movement focus options', function() {
    function makeMouseoverEvent(node) {
      var e = new MouseEvent('mouseover', {bubbles: true});
      node.dispatchEvent(e);
    }

    menu.showAt(dots);

    // Moving mouse on option 1 should focus it.
    assertNotEquals(items[0], menu.root.activeElement);
    makeMouseoverEvent(items[0]);
    assertEquals(items[0], menu.root.activeElement);

    // Moving mouse on the menu (not on option) should focus the menu.
    makeMouseoverEvent(menu);
    assertNotEquals(items[0], menu.root.activeElement);
    assertEquals(menu, document.activeElement);

    // Moving mouse on a disabled item should focus the menu.
    items[2].setAttribute('disabled', '');
    makeMouseoverEvent(items[2]);
    assertNotEquals(items[2], menu.root.activeElement);
    assertEquals(menu, document.activeElement);

    // Mouse movements should override keyboard focus.
    down();
    down();
    assertEquals(items[1], menu.root.activeElement);
    makeMouseoverEvent(items[0]);
    assertEquals(items[0], menu.root.activeElement);
  });

  test('items automatically given accessibility role', function() {
    var newItem = document.createElement('button');
    newItem.classList.add('dropdown-item');

    items[1].setAttribute('role', 'checkbox');
    menu.showAt(dots);

    return PolymerTest.flushTasks().then(() => {
      assertEquals('menuitem', items[0].getAttribute('role'));
      assertEquals('checkbox', items[1].getAttribute('role'));

      menu.insertBefore(newItem, items[0]);
      return PolymerTest.flushTasks();
    }).then(() => {
      assertEquals('menuitem', newItem.getAttribute('role'));
    });
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
    assertEquals(`${120 - menuWidth / 2}px`, menu.style.left);
    assertEquals(`${255 - menuHeight / 2}px`, menu.style.top);
    menu.close();

    // Left and top align the menu.
    menu.showAtPosition(Object.assign({}, config, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      anchorAlignmentY: AnchorAlignment.BEFORE_END,
    }));
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
    assertEquals(`${1000 - menuWidth}px`, menu.style.left);
    assertEquals(`${2000 - menuHeight}px`, menu.style.top);
    menu.close();

    // If the viewport can't fit the menu, align the menu to the viewport.
    menu.showAtPosition({
      left: menuWidth - 5,
      top: 0,
      width: 0,
      height: 0,
      maxX: menuWidth * 2 - 10,
    });
    assertEquals(`${menuWidth - 10}px`, menu.style.left);
    assertEquals(`0px`, menu.style.top);
    menu.close();

    // Alignment is reversed in RTL.
    document.body.style.direction = 'rtl';
    menu.showAtPosition(config);
    assertTrue(menu.open);
    assertEquals(140 - menuWidth, menu.offsetLeft);
    assertEquals('250px', menu.style.top);
    menu.close();
  });

  suite('offscreen scroll positioning', function() {
    var bodyHeight = 10000;
    var bodyWidth = 20000;
    var containerLeft = 5000;
    var containerTop = 10000;
    var containerWidth = 500;
    var containerHeight = 500;
    var menuWidth = 100;
    var menuHeight = 200;

    setup(function() {
      document.body.scrollTop = 0;
      document.body.scrollLeft = 0;
      document.body.innerHTML = `
        <style>
          body {
            height: ${bodyHeight}px;
            width: ${bodyWidth}px;
          }

          #container {
            overflow: auto;
            position: absolute;
            top: ${containerTop}px;
            left: ${containerLeft}px;
            right: ${containerLeft}px;
            height: ${containerHeight}px;
            width: ${containerWidth}px;
          }

          #inner-container {
            height: 1000px;
            width: 1000px;
          }

          dialog {
            height: ${menuHeight};
            width: ${menuWidth};
            padding: 0;
          }
        </style>
        <div id="container">
          <div id="inner-container">
            <button id="dots">...</button>
            <dialog is="cr-action-menu">
              <button class="dropdown-item">Un</button>
              <hr>
              <button class="dropdown-item">Dos</button>
              <button class="dropdown-item">Tres</button>
            </dialog>
          </div>
        </div>
      `;
      menu = document.querySelector('dialog[is=cr-action-menu]');
      dots = document.querySelector('#dots');
    })

    // Show the menu, scrolling the body to the button.
    test('simple offscreen', function() {
      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      assertEquals(`${containerLeft}px`, menu.style.left);
      assertEquals(`${containerTop}px`, menu.style.top);
      menu.close();
    });

    // Show the menu, scrolling the container to the button, and the body to the
    // button.
    test('offscreen and out of scroll container viewport', function() {
      document.body.scrollLeft = bodyWidth;
      document.body.scrollTop = bodyHeight;
      var container = document.querySelector('#container');

      container.scrollLeft = containerLeft;
      container.scrollTop = containerTop;

      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      assertEquals(`${containerLeft}px`, menu.style.left);
      assertEquals(`${containerTop}px`, menu.style.top);
      menu.close();
    });

    // Show the menu for an already onscreen button. The anchor should be
    // overridden so that no scrolling happens.
    test('onscreen forces anchor change', function() {
      var rect = dots.getBoundingClientRect();
      document.body.scrollLeft = rect.right - document.body.clientWidth + 10;
      document.body.scrollTop = rect.bottom - document.body.clientHeight + 10;

      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      var buttonWidth = dots.offsetWidth;
      var buttonHeight = dots.offsetHeight;
      assertEquals(containerLeft - menuWidth + buttonWidth, menu.offsetLeft);
      assertEquals(containerTop - menuHeight + buttonHeight, menu.offsetTop);
      menu.close();
    });

    test('scroll position maintained for showAtPosition', function() {
      document.body.scrollLeft = 500;
      document.body.scrollTop = 1000;
      menu.showAtPosition({top: 50, left: 50});
      assertEquals(550, menu.offsetLeft);
      assertEquals(1050, menu.offsetTop);
      menu.close();
    });

    test('rtl', function() {
      // Anchor to an item in RTL.
      document.body.style.direction = 'rtl';
      menu.showAt(dots, {anchorAlignmentX: AnchorAlignment.AFTER_START});
      assertEquals(
          container.offsetLeft + containerWidth - menuWidth,
          menu.offsetLeft);
      assertEquals(containerTop, menu.offsetTop);
      menu.close();
    });
  });
});
