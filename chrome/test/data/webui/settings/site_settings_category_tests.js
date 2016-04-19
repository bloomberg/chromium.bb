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
       * The mock proxy object to use during test.
       * @type {TestSiteSettingsPrefsBrowserProxy}
       */
      var browserProxy = null;

      /**
       * An example pref where the location category is disabled.
       * @type {SiteSettingsPref}
       */
      var prefsLocationDisabled = {
        defaults: {
          geolocation: 'block',
        },
        exceptions: {
          geolocation: [],
        },
      };

      /**
       * An example pref where the location category is enabled.
       * @type {SiteSettingsPref}
       */
      var prefsLocationEnabled = {
        defaults: {
          geolocation: 'allow',
        },
        exceptions: {
          geolocation: [],
        },
      };

      // Import necessary html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
           'chrome://md-settings/site_settings/site_settings_category.html');
      });

      // Initialize a site-settings-category before each test.
      setup(function() {
        browserProxy = new TestSiteSettingsPrefsBrowserProxy();
        settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        testElement = document.createElement('site-settings-category');
        document.body.appendChild(testElement);
      });

      test('getDefaultValueForContentType API used', function() {
        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
        return browserProxy.whenCalled('getDefaultValueForContentType').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);
            });
      });

      function testCategoryEnabled(testElement, enabled) {
        browserProxy.setPrefs(
            enabled ? prefsLocationEnabled : prefsLocationDisabled);

        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
        return browserProxy.whenCalled('getDefaultValueForContentType').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);
            assertEquals(enabled, testElement.categoryEnabled);
            MockInteractions.tap(testElement.$.toggle);
            return browserProxy.whenCalled(
                'setDefaultValueForContentType').then(
              function(arguments) {
                  assertEquals(
                      settings.ContentSettingsTypes.GEOLOCATION, arguments[0]);
                assertEquals(
                    enabled ? settings.PermissionValues.BLOCK :
                        settings.PermissionValues.ASK,
                    arguments[1]);
                assertNotEquals(enabled, testElement.categoryEnabled);
              });
          });
      }

      test('categoryEnabled correctly represents prefs (enabled)', function() {
        return testCategoryEnabled(testElement, true);
      });

      test('categoryEnabled correctly represents prefs (disabled)', function() {
        return testCategoryEnabled(testElement, false);
      });

      test('basic category tests', function() {
        for (var key in settings.ContentSettingsTypes) {
          var category = settings.ContentSettingsTypes[key];

          // All top-level categories must have category text ids and
          // descriptions. Categories that only appear under Site Details don't
          // need that.
          if (category != settings.ContentSettingsTypes.FULLSCREEN) {
            assertNotEquals('', testElement.computeCategoryTextId(category));

            assertNotEquals(
                '', testElement.computeCategoryDesc(category, true, true));
            assertNotEquals(
                '', testElement.computeCategoryDesc(category, true, false));
            assertNotEquals(
                '', testElement.computeCategoryDesc(category, false, true));
            assertNotEquals(
                '', testElement.computeCategoryDesc(category, false, false));
          }

          // All categories have an icon and a title.
          assertNotEquals(
              '', testElement.computeIconForContentCategory(category));
          assertNotEquals(
              '', testElement.computeTitleForContentCategory(category));
        }
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
