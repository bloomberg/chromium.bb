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
       * The <select> used internally by the dropdown menu.
       * @type {HTMLSelectElement}
       */
      var selectElement;

      function waitUntilDropdownUpdated() {
        return new Promise(function(resolve) { dropdown.async(resolve); });
      }

      function simulateChangeEvent(value) {
        selectElement.value = value;
        selectElement.dispatchEvent(new CustomEvent('change'));
        return waitUntilDropdownUpdated();
      }

      setup(function() {
        PolymerTest.clearBody();
        dropdown = document.createElement('settings-dropdown-menu');
        selectElement = dropdown.$$('select');
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

        return waitUntilDropdownUpdated().then(function() {
          // Initially selected item.
          assertEquals(
              'Option 100',
              selectElement.selectedOptions[0].textContent.trim());

          // Selecting an item updates the pref.
          return simulateChangeEvent('200');
        }).then(function() {
          assertEquals(200, dropdown.pref.value);

          // Updating the pref selects an item.
          dropdown.set('pref.value', 400);
          return waitUntilDropdownUpdated();
        }).then(function() {
          assertEquals('400', selectElement.value);
        });
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

        return waitUntilDropdownUpdated().then(function() {
          // Initially selected item.
          assertEquals(
              'CCC', selectElement.selectedOptions[0].textContent.trim());

          // Selecting an item updates the pref.
          return simulateChangeEvent('a');
        }).then(function() {
          assertEquals('a', dropdown.pref.value);

          // Updating the pref selects an item.
          dropdown.set('pref.value', 'b');
          return waitUntilDropdownUpdated();
        }).then(function() {
          assertEquals('b', selectElement.value);
        });
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

        return waitUntilDropdownUpdated().then(function() {
          // "Custom" initially selected.
          assertEquals(dropdown.notFoundValue_, selectElement.value);
          // Pref should not have changed.
          assertEquals('f', dropdown.pref.value);
        });
      });

      function waitForTimeout(timeMs) {
        return new Promise(function(resolve) { setTimeout(resolve, timeMs); });
      }

      test('delay setting options', function testDelayedOptions() {
        dropdown.pref = {
          key: 'test.number2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 200,
        };

        console.log('running test');
        return waitForTimeout(100).then(function() {
          return waitUntilDropdownUpdated();
        }).then(function() {
          assertTrue(dropdown.$.dropdownMenu.disabled);
          assertEquals('SETTINGS_DROPDOWN_NOT_FOUND_ITEM', selectElement.value);

          dropdown.menuOptions = [{value: 100, name: 'Option 100'},
                                  {value: 200, name: 'Option 200'},
                                  {value: 300, name: 'Option 300'},
                                  {value: 400, name: 'Option 400'}];
          return waitUntilDropdownUpdated();
        }).then(function() {
          // Dropdown menu enables itself and selects the new menu option
          // correpsonding to the pref value.
          assertFalse(dropdown.$.dropdownMenu.disabled);
          assertEquals('200', selectElement.value);
        });
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
