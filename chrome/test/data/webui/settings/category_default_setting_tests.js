// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for category-default-setting. */
cr.define('category_default_setting', function() {
  function registerTests() {
    suite('CategoryDefaultSetting', function() {
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
          geolocation: {
            setting: 'block',
          },
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
          geolocation: {
            setting: 'allow',
          },
        },
        exceptions: {
          geolocation: [],
        },
      };

      /**
       * An example pref where the Flash category is set on detect mode.
       */
      var prefsFlashDetect = {
        defaults: {
          plugins: {
            setting: 'detect_important_content',
          },
        },
        exceptions: {
          plugins: [],
        },
      };

      /**
       * An example pref where the Cookies category is set to delete when
       * session ends.
       */
      var prefsCookesSessionOnly = {
        defaults: {
          cookies: {
            setting: 'session_only',
          },
        },
        exceptions: {
          cookies: [],
        },
      };

      // Initialize a site-settings-category before each test.
      setup(function() {
        browserProxy = new TestSiteSettingsPrefsBrowserProxy();
        settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        testElement = document.createElement('category-default-setting');
        testElement.subOptionLabel = "test label";
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
        browserProxy.reset();
        browserProxy.setPrefs(
            enabled ? prefsLocationEnabled : prefsLocationDisabled);

        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
        return browserProxy.whenCalled('getDefaultValueForContentType').then(
          function(contentType) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, contentType);
            assertEquals(enabled, testElement.categoryEnabled);
            browserProxy.resetResolver('setDefaultValueForContentType');
            MockInteractions.tap(testElement.$.toggle.$.control);
            return browserProxy.whenCalled('setDefaultValueForContentType');
          }).then(function(args) {
            assertEquals(
                settings.ContentSettingsTypes.GEOLOCATION, args[0]);
            assertEquals(
                enabled ? settings.PermissionValues.BLOCK :
                    settings.PermissionValues.ASK,
                args[1]);
            assertNotEquals(enabled, testElement.categoryEnabled);
          });
      }

      test('categoryEnabled correctly represents prefs (enabled)', function() {
        return testCategoryEnabled(testElement, true);
      });

      test('categoryEnabled correctly represents prefs (disabled)', function() {
        return testCategoryEnabled(testElement, false);
      });

      function testTristateCategory(prefs, category, thirdState,
                                    secondaryToggleId) {
        browserProxy.setPrefs(prefs);

        testElement.category = category;
        var secondaryToggle = null;

        return browserProxy.whenCalled('getDefaultValueForContentType').then(
          function(contentType) {
            Polymer.dom.flush();
            secondaryToggle = testElement.$$(secondaryToggleId);
            assertTrue(!!secondaryToggle);

            assertEquals(category, contentType);
            assertTrue(testElement.categoryEnabled);
            assertFalse(secondaryToggle.disabled);
            assertTrue(secondaryToggle.checked);

            browserProxy.resetResolver('setDefaultValueForContentType');
            MockInteractions.tap(testElement.$.toggle.$.control);
            return browserProxy.whenCalled('setDefaultValueForContentType');
          }).then(function(args) {
            // Check THIRD_STATE => BLOCK transition succeeded.
            Polymer.dom.flush();

            assertEquals(category, args[0]);
            assertEquals(settings.PermissionValues.BLOCK, args[1]);
            assertFalse(testElement.categoryEnabled);
            assertTrue(secondaryToggle.disabled);
            assertTrue(secondaryToggle.checked);

            browserProxy.resetResolver('setDefaultValueForContentType');
            MockInteractions.tap(testElement.$.toggle.$.control);
            return browserProxy.whenCalled('setDefaultValueForContentType');
          }).then(function(args) {
            // Check BLOCK => THIRD_STATE transition succeeded.
            Polymer.dom.flush();

            assertEquals(category, args[0]);
            assertEquals(thirdState, args[1]);
            assertTrue(testElement.categoryEnabled);
            assertFalse(secondaryToggle.disabled);
            assertTrue(secondaryToggle.checked);

            browserProxy.resetResolver('setDefaultValueForContentType');
            MockInteractions.tap(secondaryToggle.$.control);
            return browserProxy.whenCalled('setDefaultValueForContentType');
          }).then(function(args) {
            // Check THIRD_STATE => ALLOW transition succeeded.
            Polymer.dom.flush();

            assertEquals(category, args[0]);
            assertEquals(
                settings.PermissionValues.ALLOW, args[1]);
            assertTrue(testElement.categoryEnabled);
            assertFalse(secondaryToggle.disabled);
            assertFalse(secondaryToggle.checked);

            browserProxy.resetResolver('setDefaultValueForContentType');
            MockInteractions.tap(testElement.$.toggle.$.control);
            return browserProxy.whenCalled('setDefaultValueForContentType');
          }).then(function(args) {
            // Check ALLOW => BLOCK transition succeeded.
            Polymer.dom.flush();

            assertEquals(category, args[0]);
            assertEquals(settings.PermissionValues.BLOCK, args[1]);
            assertFalse(testElement.categoryEnabled);
            assertTrue(secondaryToggle.disabled);
            assertFalse(secondaryToggle.checked);

            browserProxy.resetResolver('setDefaultValueForContentType');
            MockInteractions.tap(testElement.$.toggle.$.control);
            return browserProxy.whenCalled('setDefaultValueForContentType');
          }).then(function(args) {
            // Check BLOCK => ALLOW transition succeeded.
            Polymer.dom.flush();

            assertEquals(category, args[0]);
            assertEquals(settings.PermissionValues.ALLOW, args[1]);
            assertTrue(testElement.categoryEnabled);
            assertFalse(secondaryToggle.disabled);
            assertFalse(secondaryToggle.checked);

            browserProxy.resetResolver('setDefaultValueForContentType');
            MockInteractions.tap(secondaryToggle.$.control);
            return browserProxy.whenCalled('setDefaultValueForContentType');
          }).then(function(args) {
            // Check ALLOW => THIRD_STATE transition succeeded.
            Polymer.dom.flush();

            assertEquals(category, args[0]);
            assertEquals(thirdState, args[1]);
            assertTrue(testElement.categoryEnabled);
            assertFalse(secondaryToggle.disabled);
            assertTrue(secondaryToggle.checked);
          });
      }

      test('test special tri-state Flash category', function() {
        return testTristateCategory(
            prefsFlashDetect, settings.ContentSettingsTypes.PLUGINS,
            settings.PermissionValues.IMPORTANT_CONTENT, '#subOptionToggle');
      });

      test('test special tri-state Cookies category', function() {
        return testTristateCategory(
            prefsCookesSessionOnly, settings.ContentSettingsTypes.COOKIES,
            settings.PermissionValues.SESSION_ONLY, '#subOptionToggle');
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
