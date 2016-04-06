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

      // This promise will get resolved third, after a second display is added.
      var promise3 = new PromiseResolver();
      var onDisplayChanged2 = function() {
        fakeSystemDisplay.getInfo(function(displays) {
          Polymer.dom.flush();
          expectEquals(2, Object.keys(displayPage.displays).length);
          expectEquals(2, displays.length);
          expectEquals(displays[1].id, displayPage.displays[displays[1].id].id);
          expectEquals(displays[0].id, displayPage.selectedDisplay.id);
          expectTrue(displayPage.hasMultipleDisplays_(displayPage.displays));
          promise3.resolve();
        });
      };

      // This promise will get resolved second, after a display gets added.
      var promise2 = new PromiseResolver();
      var onDisplayChanged1 = function() {
        // Request the display info. The callback will be triggered after
        // the SettingsDisplay callback has been called.
        fakeSystemDisplay.getInfo(function(displays) {
          Polymer.dom.flush();
          expectEquals(1, Object.keys(displayPage.displays).length);
          expectEquals(1, displays.length);
          expectEquals(displays[0].id, displayPage.displays[displays[0].id].id);
          expectEquals(displays[0].id, displayPage.selectedDisplay.id);
          expectFalse(displayPage.isMirrored_(displayPage.selectedDisplay));
          expectFalse(displayPage.hasMultipleDisplays_(displayPage.displays));
          promise2.resolve();

          fakeSystemDisplay.onDisplayChanged.removeListener(onDisplayChanged1);
          fakeSystemDisplay.onDisplayChanged.addListener(onDisplayChanged2);
          addDisplay(2);
          fakeSystemDisplay.onDisplayChanged.callListeners();
        });
      };

      // This promise will get resolved first, after the initial getInfo is
      // called from SettingsDisplay.attached().
      var promise1 = fakeSystemDisplay.getInfoCalled.promise.then(function() {
        expectTrue(!!displayPage.displays);
        expectEquals(0, Object.keys(displayPage.displays).length);
        // Add a new listener, which will be called after the SettingsDisplay
        // listener is called.
        fakeSystemDisplay.onDisplayChanged.addListener(onDisplayChanged1);
        addDisplay(1);
        fakeSystemDisplay.onDisplayChanged.callListeners();
      });

      return Promise.all([promise1, promise2.promise, promise3.promise]);
    });
  });
});
