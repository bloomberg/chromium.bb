// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-settings-category. */
cr.define('site_settings_category', function() {
  function registerTests() {
    suite('SiteSettingsCategory', function() {
      /**
       * A site settings category created before each test.
       * @type {SiteSettingsCategory}
       */
      var testElement;

      /**
       * An example pref where the location category is disabled.
       */
      var prefsLocationDisabled = {
        'profile': {
          'default_content_setting_values': {
            'geolocation': {
              'value': 2,
            }
          },
        },
      };

      /**
       * An example pref where the location category is enabled.
       */
      var prefsLocationEnabled = {
        'profile': {
          'default_content_setting_values': {
            'geolocation': {
              'value': 3,
            }
          }
        },
      };

      // Import necessary html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
           'chrome://md-settings/site_settings/site_settings_category.html');
      });

      // Initialize a site-settings-category before each test.
      setup(function() {
        PolymerTest.clearBody();
        testElement = document.createElement('site-settings-category');
        document.body.appendChild(testElement);
      });

      test('categoryEnabled correctly represents prefs', function() {
        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;

        testElement.prefs = prefsLocationEnabled;
        assertTrue(testElement.categoryEnabled);
        MockInteractions.tap(testElement.$.toggle);
        assertFalse(testElement.categoryEnabled);

        testElement.prefs = prefsLocationDisabled;
        assertFalse(testElement.categoryEnabled);
        MockInteractions.tap(testElement.$.toggle);
        assertTrue(testElement.categoryEnabled);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
