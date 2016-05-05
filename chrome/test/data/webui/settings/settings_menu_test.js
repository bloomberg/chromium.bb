// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the settings menu. */

cr.define('settings_menu', function() {
  function registerSettingsMenuTest() {
    var settingsMenu = null;

    suite('SettingsMenu', function() {
      setup(function() {
        PolymerTest.clearBody();
        settingsMenu = document.createElement('settings-menu');
        document.body.appendChild(settingsMenu);
      });

      teardown(function() { settingsMenu.remove(); });

      test('upAndDownIcons', function() {
        // There should be different icons for a top level menu being open
        // vs. being closed. E.g. arrow-drop-up and arrow-drop-down.
        settingsMenu.currentRoute = {
          page: 'advanced', section: 'reset', subpage: []
        };
        var ironIconElement = settingsMenu.$.advancedPage.querySelector(
            '.menu-trigger iron-icon');
        assertTrue(!!ironIconElement);
        var openIcon = ironIconElement.icon;
        assertTrue(!!openIcon);
        // Changing to basic will close advanced.
        settingsMenu.currentRoute = {page: 'basic', section: '', subpage: []};
        assertNotEquals(openIcon, ironIconElement.icon);
      });

      test('defaultToBasic', function() {
        settingsMenu.currentRoute = {page: 'basic', section: '', subpage: []};
        assertFalse(settingsMenu.$.advancedPage.opened);
        assertTrue(settingsMenu.$.basicPage.opened);
      });

      test('openAdvanced', function() {
        settingsMenu.currentRoute = {
          page: 'advanced', section: '', subpage: []
        };
        assertTrue(settingsMenu.$.advancedPage.opened);
        assertFalse(settingsMenu.$.basicPage.opened);
      });

      test('openResetSection', function() {
        settingsMenu.currentRoute = {
          page: 'advanced', section: 'reset', subpage: []
        };
        var advancedPage = settingsMenu.$.advancedPage;
        assertTrue(advancedPage.opened);
        assertFalse(settingsMenu.$.basicPage.opened);
        assertEquals('reset',
            advancedPage.querySelector('paper-menu').selected);
      });
    });
  }

  return {
    registerTests: function() {
      registerSettingsMenuTest();
    },
  };
});
