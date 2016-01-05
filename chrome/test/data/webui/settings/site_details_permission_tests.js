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
        profile: {
          content_settings: {
            exceptions: {
              media_stream_camera: {
                value: {
                  'https:\/\/foo-allow.com:443,https:\/\/foo-allow.com:443': {
                    setting: 1,
                  }
                },
              },
            },
          },
        },
      };

      /**
       * An example empty pref.
       */
      var prefsEmpty = {
        profile: {
          content_settings: {
            exceptions: {},
          },
        },
      };

      // Import necessary html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://md-settings/site_settings/site_details_permission.html'
            );
      });

      // Initialize a site-details-permission before each test.
      setup(function() {
        PolymerTest.clearBody();
        testElement = document.createElement('site-details-permission');
        document.body.appendChild(testElement);
      });

      // Tests that the given value is converted to the expected value, for a
      // given prefType.
      var isAllowed = function(permission, prefs, origin) {
        var pref =
            testElement.prefs.profile.content_settings.exceptions[permission];
        var permissionPref = pref.value[origin + ',' + origin];
        return permissionPref.setting == settings.PermissionValues.ALLOW;
      };

      test('empty state', function() {
        testElement.prefs = prefsEmpty;
        testElement.category = settings.ContentSettingsTypes.CAMERA;
        testElement.origin = "http://www.google.com";

        assertEquals(0, testElement.offsetHeight,
            'No prefs, widget should not be visible, height');
      });

      test('camera category', function() {
        testElement.prefs = prefs;
        testElement.category = settings.ContentSettingsTypes.CAMERA;
        var origin = "https://foo-allow.com:443";
        testElement.origin = origin;

        assertNotEquals(0, testElement.offsetHeight,
            'Prefs loaded, widget should be visible, height');

        var header = testElement.$.details.querySelector('.permission-header');
        assertEquals('Camera', header.innerText.trim(),
            'Widget should be labelled correctly');

        // Flip the permission and validate that prefs stay in sync.
        assertTrue(isAllowed('media_stream_camera', prefs, origin));
        MockInteractions.tap(testElement.$.block);
        assertFalse(isAllowed('media_stream_camera', prefs, origin));
        MockInteractions.tap(testElement.$.allow);
        assertTrue(isAllowed('media_stream_camera', prefs, origin));

        // When the pref gets deleted, the widget should disappear.
        testElement.prefs = prefsEmpty;
        assertEquals(0, testElement.offsetHeight,
            'Widget should not be visible, height');
      });
    });
  }
  return {
    registerTests: registerTests,
  };
});
