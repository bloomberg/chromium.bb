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
              embeddingOrigin: 'https://foo-allow.com:443',
              origin: 'https://foo-allow.com:443',
              setting: 'allow',
              source: 'preference',
            },
          ]
        }
      };

      // Import necessary html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://md-settings/site_settings/site_details_permission.html'
            );
      });

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

      function validatePermissionFlipWorks(origin, allow) {
        MockInteractions.tap(allow ? testElement.$.allow : testElement.$.block);
        return browserProxy.whenCalled('setCategoryPermissionForOrigin').then(
            function(arguments) {
              assertEquals(origin, arguments[0]);
              assertEquals('', arguments[1]);
              assertEquals(testElement.category, arguments[2]);
              assertEquals(allow ?
                  settings.PermissionValues.ALLOW :
                  settings.PermissionValues.BLOCK, arguments[3]);
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
        }.bind(this));
      });

      test('camera category', function() {
        var origin = "https://foo-allow.com:443";
        browserProxy.setPrefs(prefs);
        testElement.category = settings.ContentSettingsTypes.CAMERA;
        testElement.site = {
          origin: origin,
          embeddingOrigin: '',
        };

        return browserProxy.whenCalled('getExceptionList').then(function() {
          assertFalse(testElement.$.details.hidden);

          var header = testElement.$.details.querySelector(
              '.permission-header');
          assertEquals('Camera', header.innerText.trim(),
              'Widget should be labelled correctly');

          // Flip the permission and validate that prefs stay in sync.
          return validatePermissionFlipWorks(origin, true).then(function() {
            browserProxy.resetResolver('setCategoryPermissionForOrigin');
            return validatePermissionFlipWorks(origin, false).then(function() {
              browserProxy.resetResolver('setCategoryPermissionForOrigin');
              return validatePermissionFlipWorks(origin, true).then(function() {
              }.bind(this));
            }.bind(this));
          }.bind(this));
        }.bind(this));
      });

      test('disappear on empty', function() {
        var origin = "https://foo-allow.com:443";
        browserProxy.setPrefs(prefs);
        testElement.category = settings.ContentSettingsTypes.CAMERA;
        testElement.site = {
          origin: origin,
          embeddingOrigin: '',
        };

        return browserProxy.whenCalled('getExceptionList').then(function() {
          assertFalse(testElement.$.details.hidden);

          browserProxy.setPrefs(prefsEmpty);
          return browserProxy.whenCalled('getExceptionList').then(function() {
            assertTrue(testElement.$.details.hidden);
          }.bind(this));
        }.bind(this));
      });
    });
  }
  return {
    registerTests: registerTests,
  };
});
