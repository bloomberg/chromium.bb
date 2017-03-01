// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for CrPolicyIndicatorBehavior. */
suite('CrPolicyIndicatorBehavior', function() {
  var TestIndicator;
  suiteSetup(function() {
    TestIndicator = Polymer({
      is: 'test-indicator',

      behaviors: [CrPolicyIndicatorBehavior],
    });
  });

  var indicator;
  setup(function() {
    indicator = new TestIndicator;
  });

  test('default indicator is blank', function() {
    assertEquals(CrPolicyIndicatorType.NONE, indicator.indicatorType)
    assertFalse(indicator.indicatorVisible);
  });

  test('policy-controlled indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.USER_POLICY;

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr20:domain', indicator.indicatorIcon);
    assertEquals('policy', indicator.indicatorTooltip);
  });

  test('recommended indicator', function() {
    indicator.indicatorType = CrPolicyIndicatorType.RECOMMENDED;

    assertTrue(indicator.indicatorVisible);
    assertEquals('cr20:domain', indicator.indicatorIcon);
    assertEquals(
        'matches',
        indicator.getIndicatorTooltip(
            indicator.indicatorType, indicator.indicatorSourceName, true));
    assertEquals(
        'differs',
        indicator.getIndicatorTooltip(
            indicator.indicatorType, indicator.indicatorSourceName, false));
  });

  if (cr.isChromeOS) {
    test('primary-user controlled indicator', function() {
      indicator.indicatorType = CrPolicyIndicatorType.PRIMARY_USER;
      indicator.indicatorSourceName = 'user@example.com';

      assertTrue(indicator.indicatorVisible);
      assertEquals('cr:group', indicator.indicatorIcon);
      assertEquals('shared: user@example.com', indicator.indicatorTooltip);
    });
  }
});
