// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-details. */
cr.define('site_details', function() {
  function registerTests() {
    suite('SiteDetails', function() {
      /**
       * A site list element created before each test.
       * @type {SiteDetails}
       */
      var testElement;

      /**
       * An example pref with 1 allowed in each category.
       */
      var prefs = {
        profile: {
          content_settings: {
            exceptions: {
              media_stream_camera: {
                value: {
                  'https:\/\/foo-allow.com:443,https:\/\/foo-allow.com:443': {
                    setting: 1,
                  },
                },
              },
              cookies: {
                value: {
                  'https:\/\/foo-allow.com:443,https:\/\/foo-allow.com:443': {
                    setting: 1,
                  },
                },
              },
              fullscreen: {
                value: {
                  'https:\/\/foo-allow.com:443,https:\/\/foo-allow.com:443': {
                    setting: 1,
                  },
                },
              },
              geolocation: {
                value: {
                  'https:\/\/foo-allow.com:443,https:\/\/foo-allow.com:443': {
                    setting: 1,
                  },
                },
              },
              javascript: {
                value: {
                  'https:\/\/foo-allow.com:443,https:\/\/foo-allow.com:443': {
                    setting: 1,
                  },
                },
              },
              media_stream_mic: {
                value: {
                  'https:\/\/foo-allow.com:443,https:\/\/foo-allow.com:443': {
                    setting: 1,
                  },
                },
              },
              notifications: {
                value: {
                  'https:\/\/foo-allow.com:443,https:\/\/foo-allow.com:443': {
                    setting: 1,
                  },
                },
              },
              popups: {
                value: {
                  'https:\/\/foo-allow.com:443,https:\/\/foo-allow.com:443': {
                    setting: 1,
                  },
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
            'chrome://md-settings/site_settings/site_details.html'
            );
      });

      // Initialize a site-details before each test.
      setup(function() {
        PolymerTest.clearBody();
        testElement = document.createElement('site-details');
        document.body.appendChild(testElement);
      });

      test('empty state', function() {
        testElement.prefs = prefsEmpty;
        testElement.origin = 'http://www.google.com';

        assertTrue(testElement.$.usage.hidden);
        assertTrue(testElement.$.storage.hidden);

        // TODO(finnur): Check for the Permission heading hiding when no
        // permissions are showing.

        var msg = 'No category should be showing, height';
        assertEquals(0, testElement.$.camera.offsetHeight, msg);
        assertEquals(0, testElement.$.cookies.offsetHeight, msg);
        assertEquals(0, testElement.$.fullscreen.offsetHeight, msg);
        assertEquals(0, testElement.$.geolocation.offsetHeight, msg);
        assertEquals(0, testElement.$.javascript.offsetHeight, msg);
        assertEquals(0, testElement.$.mic.offsetHeight, msg);
        assertEquals(0, testElement.$.notification.offsetHeight, msg);
        assertEquals(0, testElement.$.popups.offsetHeight, msg);
      });

      test('all categories visible', function() {
        testElement.prefs = prefs;
        testElement.origin = 'https://foo-allow.com:443';

        var msg = 'All categories should be showing';
        assertNotEquals(0, testElement.$.camera.offsetHeight, msg);
        assertNotEquals(0, testElement.$.cookies.offsetHeight, msg);
        assertNotEquals(0, testElement.$.fullscreen.offsetHeight, msg);
        assertNotEquals(0, testElement.$.geolocation.offsetHeight, msg);
        assertNotEquals(0, testElement.$.javascript.offsetHeight, msg);
        assertNotEquals(0, testElement.$.mic.offsetHeight, msg);
        assertNotEquals(0, testElement.$.notification.offsetHeight, msg);
        assertNotEquals(0, testElement.$.popups.offsetHeight, msg);
      });

      test('usage heading shows on storage available', function() {
        // Remove the current website-usage-private-api element.
        var parent = testElement.$.usageApi.parentNode;
        testElement.$.usageApi.remove();

        // Replace it with a mock version.
        Polymer({
          is: 'mock-website-usage-private-api',

          fetchUsageTotal: function(origin) {
            testElement.storedData_ = '1 KB';
          },
        });
        var api = document.createElement('mock-website-usage-private-api');
        testElement.$.usageApi = api;
        Polymer.dom(parent).appendChild(api);

        testElement.prefs = prefs;
        testElement.origin = 'https://foo-allow.com:443';

        assertFalse(testElement.$.usage.hidden);
        assertFalse(testElement.$.storage.hidden);
      });
    });
  }
  return {
    registerTests: registerTests,
  };
});
