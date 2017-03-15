// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('focusable-iron-list-item-behavior', function() {
  /** @type {FocusableIronListItemElement} */ var testElement;

  suiteSetup(function() {
    Polymer({
      is: 'focusable-item',
      behaviors: [FocusableIronListItemBehavior],
    });
  });

  setup(function() {
    PolymerTest.clearBody();

    testElement = document.createElement('focusable-item');
    document.body.appendChild(testElement);
  });

  test('clicking applies .no-outline', function() {
    assertFalse(testElement.classList.contains('no-outline'));
    MockInteractions.tap(testElement);
    assertTrue(testElement.classList.contains('no-outline'));
  });

  test('clicking doesn\'t apply .no-outline to key-focused item', function() {
    assertFalse(testElement.classList.contains('no-outline'));
    MockInteractions.keyUpOn(testElement);
    MockInteractions.tap(testElement);
    assertFalse(testElement.classList.contains('no-outline'));
  });

  test('clicking applies .no-outline to key-blurred item', function() {
    assertFalse(testElement.classList.contains('no-outline'));
    MockInteractions.keyUpOn(testElement);
    MockInteractions.tap(testElement);
    assertFalse(testElement.classList.contains('no-outline'));
    MockInteractions.keyDownOn(testElement);
    MockInteractions.tap(testElement);
    assertTrue(testElement.classList.contains('no-outline'));
  });

  test('"enter" key event should not propagate out of element', function() {
    var container = document.createElement('div');
    container.appendChild(testElement);
    document.body.appendChild(container);

    var reached = false;
    container.addEventListener('keydown', function() {
      reached = true;
    });

    MockInteractions.keyEventOn(testElement, 'keydown', 13, undefined, 'Enter');
    assertFalse(reached);

    // Test other keys still work.
    MockInteractions.keyEventOn(testElement, 'keydown', 9, undefined, 'Tab');
    assertTrue(reached);
  });
});
