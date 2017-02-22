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

      test('advancedOpenedBinding', function() {
        assertFalse(settingsMenu.advancedOpened);
        settingsMenu.advancedOpened = true;
        Polymer.dom.flush();
        assertTrue(settingsMenu.$.advancedSubmenu.opened);

        settingsMenu.advancedOpened = false;
        Polymer.dom.flush();
        assertFalse(settingsMenu.$.advancedSubmenu.opened);
      });

      test('tapAdvanced', function() {
        assertFalse(settingsMenu.advancedOpened);

        var advancedToggle = settingsMenu.$$('#advancedButton');
        assertTrue(!!advancedToggle);

        MockInteractions.tap(advancedToggle);
        Polymer.dom.flush();
        assertTrue(settingsMenu.$.advancedSubmenu.opened);

        MockInteractions.tap(advancedToggle);
        Polymer.dom.flush();
        assertFalse(settingsMenu.$.advancedSubmenu.opened);
      });

      test('upAndDownIcons', function() {
        // There should be different icons for a top level menu being open
        // vs. being closed. E.g. arrow-drop-up and arrow-drop-down.
        var ironIconElement = settingsMenu.$$('#advancedButton iron-icon');
        assertTrue(!!ironIconElement);

        settingsMenu.advancedOpened = true;
        Polymer.dom.flush();
        var openIcon = ironIconElement.icon;
        assertTrue(!!openIcon);

        settingsMenu.advancedOpened = false;
        Polymer.dom.flush();
        assertNotEquals(openIcon, ironIconElement.icon);
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

    suite('SettingsMenuReset', function() {
      setup(function() {
        PolymerTest.clearBody();
        settings.navigateTo(settings.Route.RESET, '');
        settingsMenu = document.createElement('settings-menu');
        document.body.appendChild(settingsMenu);
      });

      teardown(function() { settingsMenu.remove(); });

      test('openResetSection', function() {
        var selector = settingsMenu.$.subMenu;
        var path = new window.URL(selector.selected).pathname;
        assertEquals('/reset', path);
      });

      test('navigateToAnotherSection', function() {
        var selector = settingsMenu.$.subMenu;
        var path = new window.URL(selector.selected).pathname;
        assertEquals('/reset', path);

        settings.navigateTo(settings.Route.PEOPLE, '');
        Polymer.dom.flush();

        path = new window.URL(selector.selected).pathname;
        assertEquals('/people', path);
      });

      test('navigateToBasic', function() {
        var selector = settingsMenu.$.subMenu;
        var path = new window.URL(selector.selected).pathname;
        assertEquals('/reset', path);

        settings.navigateTo(settings.Route.BASIC, '');
        Polymer.dom.flush();

        // BASIC has no sub page selected.
        assertFalse(!!selector.selected);
      });
    });
  }

  return {
    registerTests: function() {
      registerSettingsMenuTest();
    },
  };
});
