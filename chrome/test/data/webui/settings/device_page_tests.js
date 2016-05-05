// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_device_page', function() {
  suite('SettingsDevicePage', function() {
    var fakePrefs = [{
      key: 'settings.touchpad.enable_tap_to_click',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    }, {
      key: 'settings.touchpad.enable_tap_dragging',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    }, {
      key: 'settings.touchpad.natural_scroll',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    }, {
      key: 'settings.language.xkb_remap_search_key_to',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 0,
    }, {
      key: 'settings.language.xkb_remap_control_key_to',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 1,
    }, {
      key: 'settings.language.xkb_remap_alt_key_to',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 2,
    }, {
      key: 'settings.language.remap_caps_lock_key_to',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 4,
    }, {
      key: 'settings.language.remap_diamond_key_to',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 3,
    }, {
      key: 'settings.language.send_function_keys',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    }];

    /** @type {!SettingsDevicePage|undefined} */
    var devicePage;

    /** @type {!FakeSystemDisplay|undefined} */
    var fakeSystemDisplay;

    setup(function() {
      fakeSystemDisplay = new settings.FakeSystemDisplay();
      settings.display.systemDisplayApi = fakeSystemDisplay;

      PolymerTest.clearBody();
      devicePage = document.createElement('settings-device-page');
      devicePage.currentRoute = {page: 'basic', section: '', subpage: []};
      devicePage.prefs = fakePrefs;
      document.body.appendChild(devicePage);
    });

    /** @return {!Element} */
    function showAndGetDeviceSubpage(subpage) {
      Polymer.dom.flush();
      var row = devicePage.$$('#main #' + subpage + 'Row');
      assertTrue(!!row);
      MockInteractions.tap(row);
      expectEquals('device', devicePage.currentRoute.section);
      expectEquals(subpage, devicePage.currentRoute.subpage[0]);
      var page = devicePage.$$('#' + subpage + ' settings-' + subpage);
      assertTrue(!!page);
      return page;
    };

    test('keyboard subpage', function() {
      // Open the keyboard subpage.
      var keyboardPage = showAndGetDeviceSubpage('keyboard');
      assertTrue(!!keyboardPage);

      // Initially, the optional keys are hidden.
      expectFalse(!!keyboardPage.$$('#capsLockKey'));
      expectFalse(!!keyboardPage.$$('#diamondKey'));

      // Pretend the diamond key is available.
      var showCapsLock = false;
      var showDiamondKey = true;
      cr.webUIListenerCallback(
          'show-keys-changed', showCapsLock, showDiamondKey);
      Polymer.dom.flush();
      expectFalse(!!keyboardPage.$$('#capsLockKey'));
      expectTrue(!!keyboardPage.$$('#diamondKey'));

      // Pretend a Caps Lock key is now available.
      showCapsLock = true;
      cr.webUIListenerCallback(
          'show-keys-changed', showCapsLock, showDiamondKey);
      Polymer.dom.flush();
      expectTrue(!!keyboardPage.$$('#capsLockKey'));
      expectTrue(!!keyboardPage.$$('#diamondKey'));
    });

    test('display subpage', function() {
      // Open the display subpage.
      var displayPage = showAndGetDeviceSubpage('display');
      assertTrue(!!displayPage);

      var addDisplay = function(n) {
        var display = {
          id: 'fakeDisplayId' + n,
          name: 'fakeDisplayName' + n,
          mirroring: '',
          isPrimary: n == 1,
          rotation: 0
        };
        fakeSystemDisplay.addDisplayForTest(display);
      };

      // First, wait for the initial call to getInfo.
      return fakeSystemDisplay.getInfoCalled.promise.then(function() {
        expectTrue(!!displayPage.displays);
        expectEquals(0, displayPage.displays.length);

        // Add a display.
        addDisplay(1);
        fakeSystemDisplay.onDisplayChanged.callListeners();

        setTimeout(function() {
          // There should be a single display which should be primary and
          // selected. Mirroring should be disabled.
          expectEquals(1, displayPage.displays.length);
          expectEquals(
              displayPage.displays[0].id, displayPage.selectedDisplay.id);
          expectEquals(
              displayPage.displays[0].id, displayPage.primaryDisplayId);
          expectFalse(displayPage.showMirror_(displayPage.displays));
          expectFalse(displayPage.isMirrored_(displayPage.displays));

          // Add a second display.
          addDisplay(2);
          fakeSystemDisplay.onDisplayChanged.callListeners();

          setTimeout(function() {
            // There should be two displays, the first should be primary and
            // selected. Mirroring should be enabled but set to false.
            expectEquals(2, displayPage.displays.length);
            expectEquals(
                displayPage.displays[0].id, displayPage.selectedDisplay.id);
            expectEquals(
                displayPage.displays[0].id, displayPage.primaryDisplayId);
            expectTrue(displayPage.showMirror_(displayPage.displays));
            expectFalse(displayPage.isMirrored_(displayPage.displays));

            // Select the second display and make it primary. Also change the
            // orientation of the second display.
            displayPage.onSelectDisplayTap_({model: {index: 1}});
            expectEquals(
                displayPage.displays[1].id, displayPage.selectedDisplay.id);

            displayPage.onMakePrimaryTap_();
            displayPage.onSetOrientation_({detail: {selected: '90'}});
            fakeSystemDisplay.onDisplayChanged.callListeners();

            setTimeout(function() {
              // Confirm that the second display is selected, primary, and
              // rotated.
              expectEquals(2, displayPage.displays.length);
              expectEquals(
                  displayPage.displays[1].id, displayPage.selectedDisplay.id);
              expectTrue(displayPage.displays[1].isPrimary);
              expectEquals(
                  displayPage.displays[1].id, displayPage.primaryDisplayId);
              expectEquals(90, displayPage.displays[1].rotation);

              // Mirror the displays.
              displayPage.onMirroredTap_();
              fakeSystemDisplay.onDisplayChanged.callListeners();

              setTimeout(function() {
                // Confirm that there is now only one display and that it
                // is primary and mirroring is enabled.
                expectEquals(1, displayPage.displays.length);
                expectEquals(
                    displayPage.displays[0].id, displayPage.selectedDisplay.id);
                expectTrue(displayPage.displays[0].isPrimary);
                expectTrue(displayPage.showMirror_(displayPage.displays));
                expectTrue(displayPage.isMirrored_(displayPage.displays));
              });
            });
          });
        });
      });
    });
  });
});
