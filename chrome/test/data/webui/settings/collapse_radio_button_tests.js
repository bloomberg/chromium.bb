// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {isChildVisible} from 'chrome://test/test_util.m.js';

// clang-format on

suite('CrCollapseRadioButton', function() {
  /** @type {SettingsCollapseRadioButtonElement} */
  let collapseRadioButton;

  setup(function() {
    PolymerTest.clearBody();
    collapseRadioButton =
        document.createElement('settings-collapse-radio-button');
    document.body.appendChild(collapseRadioButton);
    flush();
  });

  test('openOnSelection', function() {
    const collapse = collapseRadioButton.$$('iron-collapse');
    collapseRadioButton.checked = false;
    flush();
    assertFalse(collapse.opened);
    collapseRadioButton.checked = true;
    flush();
    assertTrue(collapse.opened);
  });

  test('closeOnDeselect', function() {
    const collapse = collapseRadioButton.$$('iron-collapse');
    collapseRadioButton.checked = true;
    flush();
    assertTrue(collapse.opened);
    collapseRadioButton.checked = false;
    flush();
    assertFalse(collapse.opened);
  });

  // When the button is not selected clicking the expand icon should still
  // open the iron collapse.
  test('openOnExpandHit', function() {
    const collapse = collapseRadioButton.$$('iron-collapse');
    collapseRadioButton.checked = false;
    flush();
    assertFalse(collapse.opened);
    collapseRadioButton.$$('cr-expand-button').click();
    flush();
    assertTrue(collapse.opened);
  });

  // When the button is selected clicking the expand icon should still close
  // the iron collapse.
  test('closeOnExpandHitWhenSelected', function() {
    const collapse = collapseRadioButton.$$('iron-collapse');
    collapseRadioButton.checked = true;
    flush();
    assertTrue(collapse.opened);
    collapseRadioButton.$$('cr-expand-button').click();
    flush();
    assertFalse(collapse.opened);
  });

  test('expansionHiddenWhenNoCollapseSet', function() {
    assertTrue(isChildVisible(collapseRadioButton, 'cr-expand-button'));
    assertTrue(isChildVisible(collapseRadioButton, '.separator'));

    collapseRadioButton.noCollapse = true;
    flush();
    assertFalse(isChildVisible(collapseRadioButton, 'cr-expand-button'));
    assertFalse(isChildVisible(collapseRadioButton, '.separator'));
  });

  test('openOnExpandHitWhenDisabled', function() {
    collapseRadioButton.checked = false;
    collapseRadioButton.disabled = true;
    const collapse = collapseRadioButton.$$('iron-collapse');

    flush();
    assertFalse(collapse.opened);
    collapseRadioButton.$$('cr-expand-button').click();

    flush();
    assertTrue(collapse.opened);
  });

  test('displayPolicyIndicator', function() {
    assertFalse(isChildVisible(collapseRadioButton, '#policyIndicator'));
    assertEquals(
        collapseRadioButton.policyIndicatorType, CrPolicyIndicatorType.NONE);

    collapseRadioButton.policyIndicatorType =
        CrPolicyIndicatorType.DEVICE_POLICY;
    flush();
    assertTrue(isChildVisible(collapseRadioButton, '#policyIndicator'));
  });
});
