// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  'extensions_a11y_test_fixture.js',
]);

AccessibilityTest.define('ExtensionsA11yTestFixture', {
  /** @override */
  name: 'BASIC_EXTENSIONS',

  /** @override */
  axeOptions: ExtensionsA11yTestFixture.axeOptions,

  /** @override */
  violationFilter: ExtensionsA11yTestFixture.violationFilter,

  /** @override */
  tests: {'Accessible with No Changes': function() {}},
});
