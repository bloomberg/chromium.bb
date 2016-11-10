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
        settingsMenu.currentRoute = settings.Route.BASIC;
        document.body.appendChild(settingsMenu);
      });

      teardown(function() { settingsMenu.remove(); });

      test('advancedOpenedBinding', function() {
        assertFalse(settingsMenu.advancedOpened);
        settingsMenu.advancedOpened = true;
        Polymer.dom.flush();
        assertTrue(settingsMenu.$.advancedPage.opened);

        settingsMenu.advancedOpened = false;
        Polymer.dom.flush();
        assertFalse(settingsMenu.$.advancedPage.opened);
      });

      test('tapAdvanced', function() {
        assertFalse(settingsMenu.advancedOpened);

        var advancedTrigger = settingsMenu.$$('#advancedPage .menu-trigger');
        assertTrue(!!advancedTrigger);

        MockInteractions.tap(advancedTrigger);
        Polymer.dom.flush();
        assertTrue(settingsMenu.$.advancedPage.opened);

        MockInteractions.tap(advancedTrigger);
        Polymer.dom.flush();
        assertFalse(settingsMenu.$.advancedPage.opened);
      });

      test('upAndDownIcons', function() {
        // There should be different icons for a top level menu being open
        // vs. being closed. E.g. arrow-drop-up and arrow-drop-down.
        var ironIconElement = settingsMenu.$.advancedPage.querySelector(
            '.menu-trigger iron-icon');
        assertTrue(!!ironIconElement);

        settingsMenu.advancedOpened = true;
        Polymer.dom.flush();
        var openIcon = ironIconElement.icon;
        assertTrue(!!openIcon);

        settingsMenu.advancedOpened = false;
        Polymer.dom.flush();
        assertNotEquals(openIcon, ironIconElement.icon);
      });

      test('openResetSection', function() {
        settingsMenu.currentRoute = settings.Route.RESET;
        var advancedPage = settingsMenu.$.advancedPage;
        assertEquals('/reset',
            advancedPage.querySelector('paper-menu').selected);
      });

      // Test that navigating via the paper menu always clears the current
      // search URL parameter.
      test('clearsUrlSearchParam', function() {
        var urlParams = new URLSearchParams('search=foo');
        settings.navigateTo(settings.Route.BASIC, urlParams);
        assertEquals(
            urlParams.toString(),
            settings.getQueryParameters().toString());
        MockInteractions.tap(settingsMenu.$.people);
        assertEquals('', settings.getQueryParameters().toString());
      });
    });
  }

  return {
    registerTests: function() {
      registerSettingsMenuTest();
    },
  };
});
