// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list. */
cr.define('site_list', function() {
  function registerTests() {
    suite('SiteList', function() {
      /**
       * A site list element created before each test.
       * @type {SiteList}
       */
      var testElement;

      /**
       * An example pref with 2 blocked location items and 2 allowed.
       */
      var prefs = {
        'profile': {
          'content_settings': {
            'exceptions': {
              'geolocation': {
                'value': {
                  'https:\/\/foo-allow.com:443,https:\/\/foo-allow.com:443': {
                    'setting': 1,
                  },
                  'https:\/\/bar-allow.com:443,https:\/\/bar-allow.com:443': {
                    'setting': 1,
                  },
                  'https:\/\/foo-block.com:443,https:\/\/foo-block.com:443': {
                    'setting': 2,
                  },
                  'https:\/\/bar-block.com:443,https:\/\/bar-block.com:443': {
                    'setting': 2,
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
        'profile': {
          'content_settings': {
            'exceptions': {
              'geolocation': {}
            },
          },
        },
      };

      // Import necessary html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://md-settings/site_settings/site_list.html'
            );
      });

      // Initialize a site-list before each test.
      setup(function() {
        PolymerTest.clearBody();
        testElement = document.createElement('settings-site-list');
        document.body.appendChild(testElement);
      });

      /**
       * Asserts if a menu action is incorrectly hidden.
       * @param {!HTMLElement} parentElement The parent node to start looking
       *     in.
       * @param {string} textForHiddenAction Text content of the only node that
       *     should be hidden.
       */
      function assertMenuActionHidden(parentElement, textForHiddenAction) {
        var actions = parentElement.$.listContainer.items;
        for (var i = 0; i < actions.length; ++i) {
          var content = actions[i].textContent.trim();
          if (content == textForHiddenAction)
            assertTrue(actions[i].hidden);
          else
            assertFalse(actions[i].hidden);
        }
      }

      test('Empty list', function() {
        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
        testElement.categorySubtype = settings.PermissionValues.ALLOW;
        testElement.categoryEnabled = true;
        testElement.prefs = prefsEmpty;
        testElement.initialize_();
        Polymer.dom.flush();

        assertEquals(0, testElement.sites_.length);

        assertTrue(testElement.isAllowList_());
        assertFalse(testElement.showSiteList_(testElement.sites_, true));
        assertFalse(testElement.showSiteList_(testElement.sites_, false));
        assertEquals('Allow - 0', testElement.computeSiteListHeader_(
            testElement.sites_, true));
        assertEquals('Exceptions - 0', testElement.computeSiteListHeader_(
            testElement.sites_, false));
      });

      test('initial ALLOW state is correct', function() {
        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
        testElement.categorySubtype = settings.PermissionValues.ALLOW;
        testElement.categoryEnabled = true;
        testElement.prefs = prefs;
        testElement.initialize_();
        Polymer.dom.flush();

        assertEquals(2, testElement.sites_.length);
        assertEquals('https://foo-allow.com:443', testElement.sites_[0].url);

        assertTrue(testElement.isAllowList_());
        assertMenuActionHidden(testElement, 'Allow');
        // Site list should show, no matter what category default is set to.
        assertTrue(testElement.showSiteList_(testElement.sites_, true));
        assertTrue(testElement.showSiteList_(testElement.sites_, false));
        assertEquals('Exceptions - 2', testElement.computeSiteListHeader_(
            testElement.sites_, false));
        assertEquals('Allow - 2', testElement.computeSiteListHeader_(
            testElement.sites_, true));
      });

      test('initial BLOCK state is correct', function() {
        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
        testElement.categorySubtype = settings.PermissionValues.BLOCK;
        testElement.categoryEnabled = true;
        testElement.prefs = prefs;
        testElement.initialize_();
        Polymer.dom.flush();

        assertEquals(2, testElement.sites_.length);
        assertEquals('https://foo-block.com:443', testElement.sites_[0].url);

        assertFalse(testElement.isAllowList_());
        assertMenuActionHidden(testElement, 'Block');
        // Site list should only show when category default is enabled.
        assertFalse(testElement.showSiteList_(testElement.sites_, false));
        assertTrue(testElement.showSiteList_(testElement.sites_, true));
        assertEquals('Block - 2', testElement.computeSiteListHeader_(
            testElement.sites_, true));
      });

      test('list items shown and clickable when data is present', function() {
        testElement.category = settings.ContentSettingsTypes.GEOLOCATION;
        testElement.categorySubtype = settings.PermissionValues.ALLOW;
        testElement.categoryEnabled = true;
        testElement.prefs = prefs;
        testElement.initialize_();
        // Required for firstItem to be found below.
        Polymer.dom.flush();

        // Validate that the sites_ gets populated from pre-canned prefs.
        assertEquals(2, testElement.sites_.length);
        assertEquals('https://foo-allow.com:443', testElement.sites_[0].url);
        assertEquals(undefined, testElement.selectedOrigin);

        // Validate that the sites are shown in UI and can be selected.
        var firstItem = testElement.$.listContainer.items[0];
        var clickable = firstItem.querySelector('.flex paper-item');
        assertNotEquals(undefined, clickable);
        MockInteractions.tap(clickable);
        assertEquals('https://foo-allow.com:443', testElement.selectedOrigin);
      });
    });
  }
  return {
    registerTests: registerTests,
  };
});
