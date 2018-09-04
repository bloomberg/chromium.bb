// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('base_settings_section_test', function() {
  /** @enum {string} */
  const TestNames = {
    ManagedShowsEnterpriseIcon: 'managed shows enterprise icon',
  };

  const suiteName = 'BaseSettingsSectionTest';
  suite(suiteName, function() {
    let settingsSection = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      settingsSection =
          document.createElement('print-preview-settings-section');
      document.body.appendChild(settingsSection);
    });

    // Test that key events that would result in invalid values are blocked.
    test(assert(TestNames.ManagedShowsEnterpriseIcon), function() {
      const icon = settingsSection.$$('img');

      // Default initial state is that the section is not managed, so the icon
      // is hidden.
      assertTrue(icon.hidden);
      assertFalse(settingsSection.managed);

      // Simulate setting the section to managed and verify icon appears.
      settingsSection.managed = true;
      assertFalse(icon.hidden);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
