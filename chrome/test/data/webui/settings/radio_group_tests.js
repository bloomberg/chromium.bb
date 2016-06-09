// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('SettingsRadioGroup', function() {
  /**
   * Checkbox created before each test.
   * @type {SettingsRadioGroup}
   */
  var testElement;

  /**
   * Pref value used in tests, should reflect checkbox 'checked' attribute.
   * @type {PrefObject}
   */
  var pref = {
    key: 'test',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true
  };

  suiteSetup(function() {
    return PolymerTest.importHtml('chrome://resources/polymer/v1_0/' +
                                  'paper-radio-button/paper-radio-button.html');
  });

  setup(function() {
    PolymerTest.clearBody();
    testElement = document.createElement('settings-radio-group');
    testElement.set('pref', pref);
    document.body.appendChild(testElement);
  });

  test('disables paper-radio-buttons when managed', function() {
    testElement.appendChild(document.createElement('paper-radio-button'));

    assertEquals('PAPER-RADIO-BUTTON', testElement.firstChild.tagName);
    assertFalse(testElement.firstChild.disabled);

    testElement.set('pref.policyEnforcement',
                    chrome.settingsPrivate.PolicyEnforcement.ENFORCED);
    assertTrue(testElement.firstChild.disabled);

    testElement.set('pref.policyEnforcement', undefined);
    assertFalse(testElement.firstChild.disabled);
  });
});
