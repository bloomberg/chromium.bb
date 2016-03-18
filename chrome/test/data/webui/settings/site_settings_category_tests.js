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
          location: 'block',
        },
        exceptions: {
          location: [],
        },
      };

      /**
       * An example pref where the location category is enabled.
       * @type {SiteSettingsPref}
       */
      var prefsLocationEnabled = {
        defaults: {
          location: 'allow',
        },
        exceptions: {
          location: [],
        },
      };

      // Import necessary html before running suite.
      suiteSetup(function() {
        cr.exportPath('settings_test');
        settings_test.siteCategoryNotifyForTest = true;

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

      test('getDefaultValueForContentType API used', function() {
        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
        return browserProxy.whenCalled('getDefaultValueForContentType').then(
            function(contentType) {
              assertEquals(
                  settings.ContentSettingsTypes.GEOLOCATION, contentType);
            });
      });

      test('categoryEnabled correctly represents prefs (enabled)', function() {
        return runAndResolveWhenCategoryEnabledChanged(function() {
          browserProxy.setPrefs(prefsLocationEnabled);
          testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
        }).then(function() {
          assertTrue(testElement.categoryEnabled);
          MockInteractions.tap(testElement.$.toggle);
          assertFalse(testElement.categoryEnabled);
        });
      });

      test('categoryEnabled correctly represents prefs (disabled)', function() {
        // In order for the 'change' event to trigger, the value monitored needs
        // to actually change (the event is not sent otherwise). Therefore,
        // ensure the initial state of enabledness is opposite of what we expect
        // it to end at.
        testElement.categoryEnabled = true;
        return runAndResolveWhenCategoryEnabledChanged(function() {
          browserProxy.setPrefs(prefsLocationDisabled);
          testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
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
