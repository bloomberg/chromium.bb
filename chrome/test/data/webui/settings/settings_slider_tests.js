// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-slider. */
suite('SettingsSlider', function() {
  /** @type {!SettingsSliderElement} */
  let slider;

  /**
   * cr-slider instance wrapped by settings-slider.
   * @type {!CrSliderElement}
   */
  let crSlider;

  const ticks = [2, 4, 8, 16, 32, 64, 128];

  setup(function() {
    PolymerTest.clearBody();
    slider = document.createElement('settings-slider');
    slider.pref = {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 16,
    };
    document.body.appendChild(slider);
    crSlider = slider.$$('cr-slider');
    return PolymerTest.flushTasks();
  });

  function pressArrowRight() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 39, [], 'ArrowRight');
  }

  function pressArrowLeft() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 37, [], 'ArrowLeft');
  }

  function pressPageUp() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 33, [], 'PageUp');
  }

  function pressPageDown() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 34, [], 'PageDown');
  }

  function pressArrowUp() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 38, [], 'ArrowUp');
  }

  function pressArrowDown() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 40, [], 'ArrowDown');
  }

  function pressHome() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 36, [], 'Home');
  }

  function pressEnd() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 35, [], 'End');
  }

  function pointerEvent(eventType, ratio) {
    const rect = crSlider.$.container.getBoundingClientRect();
    crSlider.dispatchEvent(new PointerEvent(eventType, {
      buttons: 1,
      pointerId: 1,
      clientX: rect.left + (ratio * rect.width),
    }));
  }

  function pointerDown(ratio) {
    pointerEvent('pointerdown', ratio);
  }

  function pointerMove(ratio) {
    pointerEvent('pointermove', ratio);
  }

  function pointerUp() {
    // Ignores clientX for pointerup event.
    pointerEvent('pointerup', 0);
  }

  function assertCloseTo(actual, expected) {
    assertTrue(
        Math.abs(1 - actual / expected) <= Number.EPSILON,
        `expected ${expected} to be close to ${actual}`);
  }

  test('enforce value', function() {
    // Test that the indicator is not present until after the pref is
    // enforced.
    indicator = slider.$$('cr-policy-pref-indicator');
    assertFalse(!!indicator);
    slider.pref = {
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 16,
    };
    Polymer.dom.flush();
    indicator = slider.$$('cr-policy-pref-indicator');
    assertTrue(!!indicator);
  });

  test('set value', function() {
    slider.ticks = ticks;
    slider.set('pref.value', 16);
    Polymer.dom.flush();
    expectEquals(6, crSlider.max);
    expectEquals(3, crSlider.value);

    // settings-slider only supports snapping to a range of tick values.
    // Setting to an in-between value should snap to an indexed value.
    slider.set('pref.value', 70);
    return PolymerTest.flushTasks()
        .then(() => {
          expectEquals(5, crSlider.value);
          expectEquals(64, slider.pref.value);

          // Setting the value out-of-range should clamp the slider.
          slider.set('pref.value', -100);
          return PolymerTest.flushTasks();
        })
        .then(() => {
          expectEquals(0, crSlider.value);
          expectEquals(2, slider.pref.value);
        });
  });

  test('move slider', function() {
    slider.ticks = ticks;
    slider.set('pref.value', 30);
    expectEquals(4, crSlider.value);

    pressArrowRight();
    expectEquals(5, crSlider.value);
    expectEquals(64, slider.pref.value);

    pressArrowRight();
    expectEquals(6, crSlider.value);
    expectEquals(128, slider.pref.value);

    pressArrowRight();
    expectEquals(6, crSlider.value);
    expectEquals(128, slider.pref.value);

    pressArrowLeft();
    expectEquals(5, crSlider.value);
    expectEquals(64, slider.pref.value);

    pressPageUp();
    expectEquals(6, crSlider.value);
    expectEquals(128, slider.pref.value);

    pressPageDown();
    expectEquals(5, crSlider.value);
    expectEquals(64, slider.pref.value);

    pressHome();
    expectEquals(0, crSlider.value);
    expectEquals(2, slider.pref.value);

    pressArrowDown();
    expectEquals(0, crSlider.value);
    expectEquals(2, slider.pref.value);

    pressArrowUp();
    expectEquals(1, crSlider.value);
    expectEquals(4, slider.pref.value);

    pressEnd();
    expectEquals(6, crSlider.value);
    expectEquals(128, slider.pref.value);
  });

  test('scaled slider', function() {
    slider.set('pref.value', 2);
    expectEquals(2, crSlider.value);

    slider.scale = 10;
    slider.max = 4;
    pressArrowRight();
    expectEquals(3, crSlider.value);
    expectEquals(.3, slider.pref.value);

    pressArrowRight();
    expectEquals(4, crSlider.value);
    expectEquals(.4, slider.pref.value);

    pressArrowRight();
    expectEquals(4, crSlider.value);
    expectEquals(.4, slider.pref.value);

    pressHome();
    expectEquals(0, crSlider.value);
    expectEquals(0, slider.pref.value);

    pressEnd();
    expectEquals(4, crSlider.value);
    expectEquals(.4, slider.pref.value);

    slider.set('pref.value', .25);
    expectEquals(2.5, crSlider.value);
    expectEquals(.25, slider.pref.value);

    pressPageUp();
    expectEquals(3.5, crSlider.value);
    expectEquals(.35, slider.pref.value);

    pressPageUp();
    expectEquals(4, crSlider.value);
    expectEquals(.4, slider.pref.value);
  });

  test('update value instantly both off and on with ticks', () => {
    slider.ticks = ticks;
    slider.set('pref.value', 2);
    slider.updateValueInstantly = false;
    assertEquals(0, crSlider.value);
    pointerDown(3 / crSlider.max);
    assertEquals(3, crSlider.value);
    assertEquals(2, slider.pref.value);
    pointerUp();
    assertEquals(3, crSlider.value);
    assertEquals(16, slider.pref.value);

    // Once |updateValueInstantly| is turned on, |value| should start updating
    // again during drag.
    pointerDown(0);
    assertEquals(0, crSlider.value);
    assertEquals(16, slider.pref.value);
    slider.updateValueInstantly = true;
    assertEquals(2, slider.pref.value);
    pointerMove(1 / crSlider.max);
    assertEquals(1, crSlider.value);
    assertEquals(4, slider.pref.value);
    slider.updateValueInstantly = false;
    pointerMove(2 / crSlider.max);
    assertEquals(2, crSlider.value);
    assertEquals(4, slider.pref.value);
    pointerUp();
    assertEquals(2, crSlider.value);
    assertEquals(8, slider.pref.value);
  });

  test('update value instantly both off and on', () => {
    slider.scale = 10;
    slider.set('pref.value', 2);
    slider.updateValueInstantly = false;
    assertCloseTo(20, crSlider.value);
    pointerDown(.3);
    assertCloseTo(30, crSlider.value);
    assertEquals(2, slider.pref.value);
    pointerUp();
    assertCloseTo(30, crSlider.value);
    assertCloseTo(3, slider.pref.value);

    // Once |updateValueInstantly| is turned on, |value| should start updating
    // again during drag.
    pointerDown(0);
    assertEquals(0, crSlider.value);
    assertCloseTo(3, slider.pref.value);
    slider.updateValueInstantly = true;
    assertEquals(0, slider.pref.value);
    pointerMove(.1);
    assertCloseTo(10, crSlider.value);
    assertCloseTo(1, slider.pref.value);
    slider.updateValueInstantly = false;
    pointerMove(.2);
    assertCloseTo(20, crSlider.value);
    assertCloseTo(1, slider.pref.value);
    pointerUp();
    assertCloseTo(20, crSlider.value);
    assertCloseTo(2, slider.pref.value);
  });
});
