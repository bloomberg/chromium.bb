// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-details. */
cr.define('site_details_permission', function() {
  function registerTests() {
    suite('SiteDetailsPermission', function() {
      /**
       * A site list element created before each test.
       * @type {SiteDetailsPermission}
       */
      var testElement;

      /**
       * An example pref with only camera allowed.
       */
      var prefs = {
        exceptions: {
          camera: [
            {
              embeddingOrigin: '',
              origin: 'https://www.example.com',
              setting: 'allow',
              source: 'preference',
            },
          ]
        }
      };

      /**
       * An example pref with only one entry allowed.
       */
      var prefsCookies = {
        exceptions: {
          cookies: [
            {
              embeddingOrigin: '',
              origin: 'https://www.example.com',
              setting: 'allow',
              source: 'preference',
            },
          ]
        }
      };

      // Initialize a site-details-permission before each test.
      setup(function() {
        browserProxy = new TestSiteSettingsPrefsBrowserProxy();
        settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        testElement = document.createElement('site-details-permission');
        document.body.appendChild(testElement);
      });

      // Tests that the given value is converted to the expected value, for a
      // given prefType.
      function isAllowed(origin, exceptionList) {
        for (var i = 0; i < exceptionList.length; ++i) {
          if (exceptionList[i].origin == origin)
            return exceptionList[i].setting == 'allow';
        }
        return false;
      };

      function validatePermissionFlipWorks(origin, expectedPermissionValue) {
        browserProxy.resetResolver('setCategoryPermissionForOrigin');

        // Simulate permission change initiated by the user.
        testElement.$.permission.value = expectedPermissionValue;
        testElement.$.permission.dispatchEvent(new CustomEvent('change'));

        return browserProxy.whenCalled('setCategoryPermissionForOrigin').then(
            function(args) {
              assertEquals(origin, args[0]);
              assertEquals('', args[1]);
              assertEquals(testElement.category, args[2]);
              assertEquals(expectedPermissionValue, args[3]);
            });
      };

      test('empty state', function() {
        browserProxy.setPrefs(prefsEmpty);
        testElement.category = settings.ContentSettingsTypes.CAMERA;
        testElement.site = {
          origin: 'http://www.google.com',
          embeddingOrigin: '',
        };

        return browserProxy.whenCalled('getExceptionList').then(function() {
          assertTrue(testElement.$.details.hidden);
        });
      });

      test('camera category', function() {
        var origin = 'https://www.example.com';
        browserProxy.setPrefs(prefs);
        testElement.category = settings.ContentSettingsTypes.CAMERA;
        testElement.label = 'Camera';
        testElement.site = {
          origin: origin,
          embeddingOrigin: '',
        };

        return browserProxy.whenCalled('getExceptionList').then(function() {
          assertFalse(testElement.$.details.hidden);

          var header = testElement.$.details.querySelector(
              '#permissionHeader');
          assertEquals('Camera', header.innerText.trim(),
              'Widget should be labelled correctly');

          // Flip the permission and validate that prefs stay in sync.
          return validatePermissionFlipWorks(
              origin, settings.PermissionValues.ALLOW);
        }).then(function() {
          return validatePermissionFlipWorks(
              origin, settings.PermissionValues.BLOCK);
        }).then(function() {
          return validatePermissionFlipWorks(
              origin, settings.PermissionValues.ALLOW);
        });
      });

      test('cookies category', function() {
        var origin = 'https://www.example.com';
        browserProxy.setPrefs(prefsCookies);
        testElement.category = settings.ContentSettingsTypes.COOKIES;
        testElement.label = 'Cookies';
        testElement.site = {
          origin: origin,
          embeddingOrigin: '',
        };

        return browserProxy.whenCalled('getExceptionList').then(function() {
          assertFalse(testElement.$.details.hidden);

          var header = testElement.$.details.querySelector(
              '#permissionHeader');
          assertEquals('Cookies', header.innerText.trim(),
              'Widget should be labelled correctly');

          return validatePermissionFlipWorks(
              origin, settings.PermissionValues.SESSION_ONLY);
        }).then(function() {
          // Flip the permission and validate that prefs stay in sync.
          return validatePermissionFlipWorks(
              origin, settings.PermissionValues.ALLOW);
        }).then(function() {
          return validatePermissionFlipWorks(
              origin, settings.PermissionValues.BLOCK);
        });
      });

      test('disappear on empty', function() {
        var origin = "https://www.example.com";
        browserProxy.setPrefs(prefs);
        testElement.category = settings.ContentSettingsTypes.CAMERA;
        testElement.site = {
          origin: origin,
          embeddingOrigin: '',
        };

        return browserProxy.whenCalled('getExceptionList').then(function() {
          assertFalse(testElement.$.details.hidden);

          browserProxy.setPrefs(prefsEmpty);
          return browserProxy.whenCalled('getExceptionList');
        }).then(function() {
          assertTrue(testElement.$.details.hidden);
        });
      });
    });
  }
  return {
    registerTests: registerTests,
  };
});
