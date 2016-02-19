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
        profile: {
          default_content_setting_values: {
            geolocation: {
              value: 2,
            }
          },
        },
      };

      /**
       * An example pref where the location category is enabled.
       */
      var prefsLocationEnabled = {
        profile: {
          default_content_setting_values: {
            geolocation: {
              value: 3,
            }
          }
        },
      };

      // Import necessary html before running suite.
      suiteSetup(function() {
        cr.define('settings_test', function() {
          var siteSettingsCategoryOptions = {
            /**
             * True if property changes should fire events for testing purposes.
             * @type {boolean}
             */
            notifyPropertyChangesForTest: true,
          };
          return {siteSettingsCategoryOptions: siteSettingsCategoryOptions};
        });

        return PolymerTest.importHtml(
           'chrome://md-settings/site_settings/site_settings_category.html');
      });

      // Initialize a site-settings-category before each test.
      setup(function() {
        PolymerTest.clearBody();
        testElement = document.createElement('site-settings-category');
        document.body.appendChild(testElement);
      });

      /**
       * Returns a promise that resolves once the selected item is updated.
       * @param {function()} action is executed after the listener is set up.
       * @return {!Promise} a Promise fulfilled when the selected item changes.
       */
      function runAndResolveWhenCategoryEnabledChanged(action) {
        return new Promise(function(resolve, reject) {
          var handler = function() {
            testElement.removeEventListener(
                'category-enabled-changed', handler);
            resolve();
          };
          testElement.addEventListener('category-enabled-changed', handler);
          action();
        });
      }

      test('categoryEnabled correctly represents prefs (enabled)', function() {
        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;

        return runAndResolveWhenCategoryEnabledChanged(function() {
          testElement.prefs = prefsLocationEnabled;
        }).then(function() {
          assertTrue(testElement.categoryEnabled);
          MockInteractions.tap(testElement.$.toggle);
          assertFalse(testElement.categoryEnabled);
        });
      });

      test('categoryEnabled correctly represents prefs (disabled)', function() {
        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;

        // In order for the 'change' event to trigger, the value monitored needs
        // to actually change (the event is not sent otherwise). Therefore,
        // ensure the initial state of enabledness is opposite of what we expect
        // it to end at.
        testElement.categoryEnabled = true;
        return runAndResolveWhenCategoryEnabledChanged(function() {
          testElement.prefs = prefsLocationDisabled;
        }).then(function() {
          assertFalse(testElement.categoryEnabled);
          MockInteractions.tap(testElement.$.toggle);
          assertTrue(testElement.categoryEnabled);
        });
      });

      test('basic category tests', function() {
        for (var key in settings.ContentSettingsTypes) {
          var category = settings.ContentSettingsTypes[key];

          // All categories have a textId, an icon, a title, and pref names.
          assertNotEquals('', testElement.computeCategoryTextId(category));
          assertNotEquals(
              '', testElement.computeIconForContentCategory(category));
          assertNotEquals(
              '', testElement.computeTitleForContentCategory(category));
          assertNotEquals(
              '', testElement.computeCategoryPrefName(category));
          assertNotEquals(
              '', testElement.computeCategoryExceptionsPrefName(category));

          assertNotEquals(
              '', testElement.computeCategoryDesc(category, true, true));
          assertNotEquals(
              '', testElement.computeCategoryDesc(category, true, false));
          assertNotEquals(
              '', testElement.computeCategoryDesc(category, false, true));
          assertNotEquals(
              '', testElement.computeCategoryDesc(category, false, false));
        }
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
