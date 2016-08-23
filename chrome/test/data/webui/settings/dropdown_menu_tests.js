// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-dropdown-menu. */
cr.define('settings_dropdown_menu', function() {
  function registerTests() {
    suite('SettingsDropdownMenu', function() {
      /** @type {SettingsDropdownMenu} */
      var dropdown;

      /**
       * The IronSelectable (paper-listbox) used internally by the dropdown
       * menu.
       * @type {Polymer.IronSelectableBehavior}
       */
      var selectable;

      setup(function() {
        PolymerTest.clearBody();
        dropdown = document.createElement('settings-dropdown-menu');
        selectable = dropdown.$$('paper-listbox');
        document.body.appendChild(dropdown);
      });

      test('with number options', function testNumberOptions() {
        dropdown.pref = {
          key: 'test.number',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 100,
        };
        dropdown.menuOptions = [{value: 100, name: 'Option 100'},
                                {value: 200, name: 'Option 200'},
                                {value: 300, name: 'Option 300'},
                                {value: 400, name: 'Option 400'}];

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
            [{value: 'a', name: 'AAA'},
             {value: 'b', name: 'BBB'},
             {value: 'c', name: 'CCC'},
             {value: 'd', name: 'DDD'}];
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
            [{value: 'a', name: 'AAA'},
             {value: 'b', name: 'BBB'},
             {value: 'c', name: 'CCC'},
             {value: 'd', name: 'DDD'}];
        Polymer.dom.flush();

        // "Custom" initially selected.
        assertEquals(dropdown.notFoundValue_, selectable.selected);
        // Pref should not have changed.
        assertEquals('f', dropdown.pref.value);
      });

      test('delay setting options', function testDelayedOptions(done) {
        dropdown.pref = {
          key: 'test.number2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 200,
        };

        setTimeout(function() {
          assertEquals(
              loadTimeData.getValue('loading'), dropdown.$.dropdownMenu.label);
          assertTrue(dropdown.$.dropdownMenu.disabled);
          assertEquals(undefined, selectable.selected);

          dropdown.menuOptions = [{value: 100, name: 'Option 100'},
                                  {value: 200, name: 'Option 200'},
                                  {value: 300, name: 'Option 300'},
                                  {value: 400, name: 'Option 400'}];
          Polymer.dom.flush();

          // Dropdown menu enables itself and selects the new menu option
          // correpsonding to the pref value.
          assertFalse(dropdown.$.dropdownMenu.disabled);
          assertEquals('200', selectable.selected);
          done();
        }, 100);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
