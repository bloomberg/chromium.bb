// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr_policy-network-indicator. */
suite('cr-policy-network-indicator', function() {
  /** @type {!CrPolicyNetworkIndicatorElement|undefined} */
  var indicator;

  /** @type {!PaperTooltipElement|undefined} */
  var tooltip;

  setup(function() {
    PolymerTest.clearBody();

    indicator = document.createElement('cr-policy-network-indicator');
    document.body.appendChild(indicator);
    tooltip = indicator.$$('paper-tooltip');
  });

  teardown(function() {
    PolymerTest.clearBody();  // crbug.com/680169
  });

  test('hidden by default', function() {
    assertTrue(indicator.$.indicator.hidden);
  });

  test('no policy', function() {
    indicator.property = {Active: 'foo'};
    Polymer.dom.flush();
    assertTrue(indicator.$.indicator.hidden);
  });

  test('recommended', function() {
    indicator.property = {
      Active: 'foo',
      UserPolicy: 'bar',
      UserEditable: true,
      Effective: 'UserPolicy',
    };
    Polymer.dom.flush();
    assertFalse(indicator.$.indicator.hidden);
    assertEquals('cr20:domain', indicator.$.indicator.icon);
    assertEquals('differs', tooltip.textContent.trim());

    indicator.set('property.Active', 'bar');
    Polymer.dom.flush();
    assertEquals('matches', tooltip.textContent.trim());
  });

  test('policy', function() {
    indicator.property = {
      DevicePolicy: 'foo',
      Effective: 'DevicePolicy',
    };
    Polymer.dom.flush();
    assertFalse(indicator.$.indicator.hidden);
    assertEquals('cr20:domain', indicator.$.indicator.icon);
    assertEquals('policy', tooltip.textContent.trim());
  });

  test('extension', function() {
    indicator.property = {
      Active: 'foo',
      Effective: 'ActiveExtension',
    };
    Polymer.dom.flush();
    assertEquals('cr:extension', indicator.$.indicator.icon);
  });
});
