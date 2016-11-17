// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('controlled button', function() {
  /** @type {ControlledButtonElement} */
  var button;

  /** @type {!chrome.settingsPrivate.PrefObject} */
  var pref = {
    key: 'test',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true
  };

  setup(function() {
    PolymerTest.clearBody();
    button = document.createElement('controlled-button');
    button.pref = pref;
    document.body.appendChild(button);
  });

  test('disables when pref is managed', function() {
    button.set('pref.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
    Polymer.dom.flush();
    assertTrue(button.$$('paper-button').disabled);

    var indicator = button.$$('cr-policy-pref-indicator');
    assertTrue(!!indicator);
    assertGT(indicator.clientHeight, 0);

    button.set('pref.enforcement', undefined);
    Polymer.dom.flush();
    assertFalse(button.$$('paper-button').disabled);
    assertEquals(0, indicator.clientHeight);
  });
});
