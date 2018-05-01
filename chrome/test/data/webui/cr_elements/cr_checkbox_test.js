// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-checkbox', function() {
  let checkbox;

  setup(function() {
    PolymerTest.clearBody();
    checkbox = document.createElement('cr-checkbox');
    document.body.appendChild(checkbox);
    assertNotChecked();
  });

  function assertChecked() {
    assertTrue(checkbox.checked);
    assertTrue(checkbox.hasAttribute('checked'));
    assertEquals('true', checkbox.getAttribute('aria-checked'));
  }

  function assertNotChecked() {
    assertFalse(checkbox.checked);
    assertEquals(null, checkbox.getAttribute('checked'));
    assertEquals('false', checkbox.getAttribute('aria-checked'));
  }

  function assertDisabled() {
    assertTrue(checkbox.disabled);
    assertEquals('-1', checkbox.getAttribute('tabindex'));
    assertTrue(checkbox.hasAttribute('disabled'));
    assertEquals('true', checkbox.getAttribute('aria-disabled'));
    assertEquals('none', getComputedStyle(checkbox).pointerEvents);
  }

  function assertNotDisabled() {
    assertFalse(checkbox.disabled);
    assertEquals('0', checkbox.getAttribute('tabindex'));
    assertFalse(checkbox.hasAttribute('disabled'));
    assertEquals('false', checkbox.getAttribute('aria-disabled'));
  }

  /** @param {string} keyName The name of the key to trigger. */
  function triggerKeyPressEvent(keyName) {
    // Note: MockInteractions incorrectly populates |keyCode| and |code| with
    // the same value. Since the prod code only cares about |code| being 'Enter'
    // or 'Space', passing a string as a 2nd param, instead of a number.
    MockInteractions.keyEventOn(
        checkbox, 'keypress', keyName, undefined, keyName);
  }

  // Test that the control is checked when the user taps on it (no movement
  // between pointerdown and pointerup).
  test('ToggleByMouse', function() {
    let whenChanged = test_util.eventToPromise('change', checkbox);
    checkbox.click();
    return whenChanged
        .then(function() {
          assertChecked();
          whenChanged = test_util.eventToPromise('change', checkbox);
          checkbox.click();
          return whenChanged;
        })
        .then(function() {
          assertNotChecked();
        });
  });

  // Test that the control is checked when the |checked| attribute is
  // programmatically changed.
  test('ToggleByAttribute', function(done) {
    test_util.eventToPromise('change', checkbox).then(function() {
      // Should not fire 'change' event when state is changed programmatically.
      // Only user interaction should result in 'change' event.
      assertFalse(true);
    });

    checkbox.checked = true;
    assertChecked();

    checkbox.checked = false;
    assertNotChecked();

    setTimeout(done);
  });

  // Test that the control is checked when the user presses the 'Enter' or
  // 'Space' key.
  test('ToggleByKey', function() {
    let whenChanged = test_util.eventToPromise('change', checkbox);
    triggerKeyPressEvent('Enter');
    return whenChanged
        .then(function() {
          assertChecked();
          whenChanged = test_util.eventToPromise('change', checkbox);
          triggerKeyPressEvent('Space');
        })
        .then(function() {
          assertNotChecked();
        });
  });

  // Test that the control is not affected by user interaction when disabled.
  test('ToggleWhenDisabled', function(done) {
    assertNotDisabled();
    checkbox.disabled = true;
    assertDisabled();

    test_util.eventToPromise('change', checkbox).then(function() {
      assertFalse(true);
    });

    checkbox.click();
    triggerKeyPressEvent('Enter');
    setTimeout(done);
  });
});
