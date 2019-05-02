// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('split_settings_flag', function() {
  suite('splitSettingsFlag', function() {
    const attachedSections = new Set();

    /**
     * Names of settings sections that affect Chrome browser (and possibly CrOS)
     * and therefore should appear in browser settings even when SettingsSplit
     * feature is enabled.
     * @type {!Array<string>}
     */
    const BROWSER_SETTINGS_SECTIONS = [
      'a11y',
      'appearance',
      'autofill',
      'languages',
      'onStartup',
      'printing',
      'privacy',
      'search',
    ];

    /**
     * Names of settings sections that affect only CrOS (i.e. not the Chrome
     * broswer) and therefore should not appear in browser settings when
     * SettingsSplit feature is enabled.
     * @type {!Array<string>}
     */
    const OS_SETTINGS_ONLY_SECTIONS = [
      'androidApps',
      'bluetooth',
      'changePassword',
      'crostini',
      'date-time',
      'device',
      'internet',
      'kiosk-next-shell',
      'multidevice',
      'people',
    ];

    const UNMIGRATED_SECTIONS = [
      'changePassword',
      'date-time',
      'people',
      'reset',
    ];

    setup(async function() {
      PolymerTest.clearBody();
      const browserSettings = document.createElement('settings-basic-page');
      // In prod, page visibility is passed via Polymer binding layers but it
      // is always set to settings.pageVisibility.
      browserSettings.pageVisibility = settings.pageVisibility;
      document.body.appendChild(browserSettings);
      Polymer.dom.flush();

      // Expand <settings-idle-load> containing advanced section
      await browserSettings.$$('#advancedPageTemplate').get();

      browserSettings.shadowRoot.querySelectorAll('settings-section')
          .forEach(element => attachedSections.add(element.section));
    });

    test(
        'Browser settings page contains all browser-relevant settings',
        function() {
          for (let section of BROWSER_SETTINGS_SECTIONS) {
            assertTrue(attachedSections.has(section));
          }
        });

    test(
        'Browser settings page contains no migrated CrOS-only settings',
        function() {
          const attachedOsSections =
              OS_SETTINGS_ONLY_SECTIONS.filter(e => attachedSections.has(e));
          for (let section of attachedOsSections) {
            assertTrue(UNMIGRATED_SECTIONS.includes(section));
          }
        });
  });
});
