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
        settingsMenu.currentRoute = {
          page: 'basic', section: '', subpage: []
        };
        document.body.appendChild(settingsMenu);
      });

      teardown(function() { settingsMenu.remove(); });

      test('openAdvanced', function() {
        settingsMenu.fire('toggle-advanced-page', true);
        Polymer.dom.flush();
        assertTrue(settingsMenu.$.advancedPage.opened);
      });

      test('upAndDownIcons', function() {
        // There should be different icons for a top level menu being open
        // vs. being closed. E.g. arrow-drop-up and arrow-drop-down.
        var ironIconElement = settingsMenu.$.advancedPage.querySelector(
            '.menu-trigger iron-icon');
        assertTrue(!!ironIconElement);

        settingsMenu.fire('toggle-advanced-page', true);
        Polymer.dom.flush();
        var openIcon = ironIconElement.icon;
        assertTrue(!!openIcon);

        settingsMenu.fire('toggle-advanced-page', false);
        Polymer.dom.flush();
        assertNotEquals(openIcon, ironIconElement.icon);
      });

      test('openResetSection', function() {
        settingsMenu.currentRoute = {
          page: 'advanced', section: 'reset', subpage: []
        };
        var advancedPage = settingsMenu.$.advancedPage;
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
