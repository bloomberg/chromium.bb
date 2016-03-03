// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_device_page', function() {
  /** @return {!DevicePageElement} */
  function getDevicePage() {
    var devicePage = document.createElement('settings-device-page');
    var page = this.getPage('basic');
    var deviceSection = this.getSection(page, 'device');
    expectTrue(!!deviceSection);
    var devicePage = deviceSection.querySelector('settings-device-page');
    expectTrue(!!devicePage);
    return devicePage;
  };

  suite('SettingsDevicePage', function() {
    /** @type {CrSettingsPrefsElement} */
    var settingsPrefs = null;

    /** @type {settings.FakeSettingsPrivate} */
    var fakeApi = null;

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

    // Initialize <settings-device-page> before each test.
    setup(function() {
      CrSettingsPrefs.deferInitialization = true;
      fakeApi = new settings.FakeSettingsPrivate(fakePrefs);
      settingsPrefs = document.createElement('settings-prefs');
      settingsPrefs.initializeForTesting(fakeApi);
      return CrSettingsPrefs.initialized;
    });

    test('keyboard subpage', function() {
      var devicePage = document.createElement('settings-device-page');
      devicePage.currentRoute = {page: 'basic', section: '', subpage: []};
      devicePage.prefs = settingsPrefs.prefs;

      // Open the keyboard subpage.
      var keyboardRow = devicePage.$$('#main #keyboardRow');
      assertTrue(!!keyboardRow);
      MockInteractions.tap(keyboardRow);
      expectEquals(devicePage.currentRoute.section, 'device');
      expectEquals(devicePage.currentRoute.subpage[0], 'keyboard');
      var keyboardPage = devicePage.$$('#keyboard settings-keyboard');
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
  });
});
