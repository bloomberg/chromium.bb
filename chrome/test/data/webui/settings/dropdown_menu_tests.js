// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-dropdown-menu. */
cr.define('settings_dropdown_menu', function() {
  function registerTests() {
    suite('SettingsDropdownMenu', function() {
      // Import settings_dropdown_menu.html before running suite.
      suiteSetup(function() {
        return Promise.all([
          PolymerTest.importHtml('chrome://md-settings/i18n_setup.html'),
          PolymerTest.importHtml(
              'chrome://md-settings/controls/settings_dropdown_menu.html'),
        ]);
      });

      /** @type {SettingsDropdownMenu} */
      var dropdown;

      /**
       * The IronSelectable (paper-menu) used internally by the dropdown menu.
       * @type {Polymer.IronSelectableBehavior}
       */
      var selectable;

      setup(function() {
        PolymerTest.clearBody();
        dropdown = document.createElement('settings-dropdown-menu');
        selectable = dropdown.$$('paper-menu');
        document.body.appendChild(dropdown);
      });

      test('with number options', function testNumberOptions() {
        dropdown.pref = {
          key: 'test.number',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 100,
        };
        dropdown.menuOptions = [[100, 'Option 100'], [200, 'Option 200'],
                                [300, 'Option 300'], [400, 'Option 400']];

        // IronSelectable uses a DOM observer, which uses a debouncer.
        Polymer.dom.flush();

        // Initially selected item.
        assertEquals('Option 100', selectable.selectedItem.textContent.trim());

        // Selecting an item updates the pref.
        selectable.selected = '200';
        assertEquals(200, dropdown.pref.value);

        // Updating the pref selects an item.
        dropdown.set('pref.value', 400);
        assertEquals('400', selectable.selected);
      });

      test('with string options', function testStringOptions() {
        dropdown.pref = {
          key: 'test.string',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: 'c',
        };
        dropdown.menuOptions =
            [['a', 'AAA'], ['b', 'BBB'], ['c', 'CCC'], ['d', 'DDD']];
        Polymer.dom.flush();

        // Initially selected item.
        assertEquals('CCC', selectable.selectedItem.textContent.trim());

        // Selecting an item updates the pref.
        selectable.selected = 'a';
        assertEquals('a', dropdown.pref.value);

        // Updating the pref selects an item.
        dropdown.set('pref.value', 'b');
        assertEquals('b', selectable.selected);
      });

      test('with custom value', function testCustomValue() {
        dropdown.pref = {
          key: 'test.string',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: 'f',
        };
        dropdown.menuOptions =
            [['a', 'AAA'], ['b', 'BBB'], ['c', 'CCC'], ['d', 'DDD']];
        Polymer.dom.flush();

        // "Custom" initially selected.
        assertEquals(dropdown.notFoundValue_, selectable.selected);
        // Pref should not have changed.
        assertEquals('f', dropdown.pref.value);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
