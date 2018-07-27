// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Multidevice', () => {
  let featureToggle = null;

  function click() {
    featureToggle.$$('cr-toggle').click();
    Polymer.dom.flush();
  }

  // Initialize a checked control before each test.
  setup(() => {
    /**
     * Pref value used in tests, should reflect the 'checked' attribute.
     * Create a new pref for each test() to prevent order (state)
     * dependencies between tests.
     * @type {chrome.settingsPrivate.PrefObject}
     */
    const pref = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true
    };
    PolymerTest.clearBody();

    featureToggle =
        document.createElement('settings-multidevice-feature-toggle');
    featureToggle.set('pref', pref);
    document.body.appendChild(featureToggle);
  });

  test('value changes on click', () => {
    assertTrue(featureToggle.checked);
    assertTrue(featureToggle.pref.value);

    click();
    assertFalse(featureToggle.checked);
    assertFalse(featureToggle.pref.value);

    click();
    assertTrue(featureToggle.checked);
    assertTrue(featureToggle.pref.value);
  });

  test('fires a change event', (done) => {
    featureToggle.addEventListener('change', () => {
      assertFalse(featureToggle.checked);
      done();
    });
    assertTrue(featureToggle.checked);
    click();
  });

  test('does not change when disabled', () => {
    featureToggle.checked = false;
    featureToggle.setAttribute('disabled', true);
    assertTrue(featureToggle.disabled);
    assertTrue(featureToggle.$$('cr-toggle').disabled);

    click();
    assertFalse(featureToggle.checked);
    assertFalse(featureToggle.$$('cr-toggle').checked);
  });

  test('fires event but leaves pref when noSetPref is true ', () => {
    let eventFired = false;
    document.addEventListener(
        'settings-boolean-control-change', () => eventFired = true);
    featureToggle.noSetPref = true;

    click();
    assertTrue(featureToggle.checked);
    assertTrue(featureToggle.pref.value);
    assertTrue(eventFired);
  });
});
