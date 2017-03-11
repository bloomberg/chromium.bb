// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-policy-indicator. */
suite('CrPolicyIndicator', function() {
  /** @type {!CrPolicyIndicatorElement|undefined} */
  var indicator;

  /** @type {!PaperTooltipElement|undefined} */
  var tooltip;

  setup(function() {
    PolymerTest.clearBody();

    indicator = document.createElement('cr-policy-indicator');
    document.body.appendChild(indicator);
    tooltip = indicator.$$('paper-tooltip');
  });

  teardown(function() {
    PolymerTest.clearBody();  // crbug.com/680169
  });

  test('none', function() {
    assertTrue(indicator.$.indicator.hidden);
  });

  test('indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.USER_POLICY;

    assertFalse(indicator.$.indicator.hidden);
    assertEquals('cr20:domain', indicator.$.indicator.icon);
    assertEquals('policy', tooltip.textContent.trim());

    if (cr.isChromeOS) {
      indicator.indicatorType = CrPolicyIndicatorType.OWNER;
      indicator.indicatorSourceName = 'foo@example.com';

      assertEquals('cr:person', indicator.$.indicator.icon);
      assertEquals('owner: foo@example.com', tooltip.textContent.trim());
    }
  });
});
