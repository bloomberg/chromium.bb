// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr_policy-pref-indicator. */
suite('CrPolicyPrefIndicator', function() {
  /** @type {!CrPolicyPrefIndicatorElement|undefined} */
  var indicator;

  /** @type {!PaperTooltipElement|undefined} */
  var tooltip;

  setup(function() {
    PolymerTest.clearBody();

    indicator = document.createElement('cr-policy-pref-indicator');
    document.body.appendChild(indicator);
    tooltip = indicator.$$('paper-tooltip');
  });

  teardown(function() {
    PolymerTest.clearBody();  // crbug.com/680169
  });

  test('none', function() {
    assertTrue(indicator.$.indicator.hidden);
  });

  test('pref', function() {
    /** @type {!chrome.settingsPrivate.PrefObject} */
    indicator.pref = {
      key: 'foo',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };
    Polymer.dom.flush();
    assertTrue(indicator.$.indicator.hidden);

    indicator.set(
        'pref.controlledBy', chrome.settingsPrivate.ControlledBy.OWNER);
    indicator.set('pref.controlledByName', 'owner_name');
    indicator.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
    Polymer.dom.flush();
    assertFalse(indicator.$.indicator.hidden);
    assertEquals('cr:person', indicator.$.indicator.icon);
    assertEquals('owner: owner_name', tooltip.textContent.trim());

    indicator.set('pref.value', 'foo');
    indicator.set('pref.recommendedValue', 'bar');
    indicator.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.RECOMMENDED);
    Polymer.dom.flush();
    assertFalse(indicator.$.indicator.hidden);
    assertEquals('cr20:domain', indicator.$.indicator.icon);
    assertEquals('differs', tooltip.textContent.trim());

    indicator.set('pref.value', 'bar');
    Polymer.dom.flush();
    assertEquals('matches', tooltip.textContent.trim());
  });
});
