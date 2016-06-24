// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('device_page_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Display: 'display',
    Keyboard: 'keyboard',
    Touchpad: 'touchpad',
  };

  /**
   * @constructor
   * @implements {settings.DevicePageBrowserProxy}
   */
  function TestDevicePageBrowserProxy() {
    this.keyboardShortcutsOverlayShown_ = 0;
  }

  TestDevicePageBrowserProxy.prototype = {
    /** @override */
    handleLinkEvent: function(e) {
      settings.DevicePageBrowserProxyImpl.prototype.handleLinkEvent.call(
          this, e);
      // Prevent opening the link, which can block the test.
      e.preventDefault();
    },

    /** override */
    initializeKeyboard: function() {},

    /** override */
    showKeyboardShortcutsOverlay: function() {
      this.keyboardShortcutsOverlayShown_++;
    },
  };

  suite('SettingsDevicePage', function() {
    var fakePrefs = {
      settings: {
        touchpad: {
          enable_tap_to_click: {
            key: 'settings.touchpad.enable_tap_to_click',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
          enable_tap_dragging: {
            key: 'settings.touchpad.enable_tap_dragging',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
          natural_scroll: {
            key: 'settings.touchpad.natural_scroll',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
        language: {
          xkb_remap_search_key_to: {
            key: 'settings.language.xkb_remap_search_key_to',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
          xkb_remap_control_key_to: {
            key: 'settings.language.xkb_remap_control_key_to',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 1,
          },
          xkb_remap_alt_key_to: {
            key: 'settings.language.xkb_remap_alt_key_to',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 2,
          },
          remap_caps_lock_key_to: {
            key: 'settings.language.remap_caps_lock_key_to',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 4,
          },
          remap_diamond_key_to: {
            key: 'settings.language.remap_diamond_key_to',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 3,
          },
          send_function_keys: {
            key: 'settings.language.send_function_keys',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          xkb_auto_repeat_enabled_r2: {
            key: 'prefs.settings.language.xkb_auto_repeat_enabled_r2',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
          xkb_auto_repeat_delay_r2: {
            key: 'settings.language.xkb_auto_repeat_delay_r2',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 500,
          },
          xkb_auto_repeat_interval_r2: {
            key: 'settings.language.xkb_auto_repeat_interval_r2',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 500,
          },
        }
      }
    };

    /** @type {!SettingsDevicePage|undefined} */
    var devicePage;

    /** @type {!FakeSystemDisplay|undefined} */
    var fakeSystemDisplay;

    suiteSetup(function() {
      // Disable animations so sub-pages open within one event loop.
      testing.Test.disableAnimationsAndTransitions();
    });

    setup(function(done) {
      fakeSystemDisplay = new settings.FakeSystemDisplay();
      settings.display.systemDisplayApi = fakeSystemDisplay;

      PolymerTest.clearBody();
      devicePage = document.createElement('settings-device-page');
      devicePage.currentRoute = {page: 'basic', section: '', subpage: []};
      devicePage.prefs = fakePrefs;
      settings.DevicePageBrowserProxyImpl.instance_ =
          new TestDevicePageBrowserProxy();
      document.body.appendChild(devicePage);

      // Allow the light DOM to be distributed to settings-animated-pages.
      setTimeout(done);
    });

    /** @return {!Promise<!Element>} */
    function showAndGetDeviceSubpage(subpage) {
      Polymer.dom.flush();
      var row = devicePage.$$('#main #' + subpage + 'Row');
      assertTrue(!!row);
      MockInteractions.tap(row);

      // The 0-duration animation still requires flushing the task queue.
      return new Promise(function(resolve, reject) {
        expectEquals('device', devicePage.currentRoute.section);
        expectEquals(subpage, devicePage.currentRoute.subpage[0]);
        var page = devicePage.$$('#' + subpage + ' settings-' + subpage);
        assertTrue(!!page);
        setTimeout(function() {
          resolve(page);
        });
      });
    };

    /**
     * @param {!HTMLElement} touchpadPage
     * @param {Boolean} expected
     */
    function expectNaturalScrollValue(touchpadPage, expected) {
      var naturalScrollOff =
          touchpadPage.$$('paper-radio-button[name="false"]');
      var naturalScrollOn =
          touchpadPage.$$('paper-radio-button[name="true"]');
      assertTrue(!!naturalScrollOff);
      assertTrue(!!naturalScrollOn);

      expectEquals(!expected, naturalScrollOff.checked);
      expectEquals(expected, naturalScrollOn.checked);
      expectEquals(expected,
                   devicePage.prefs.settings.touchpad.natural_scroll.value);
    }

    test(assert(TestNames.Touchpad), function(done) {
      showAndGetDeviceSubpage('touchpad').then(function(touchpadPage) {
        expectNaturalScrollValue(touchpadPage, false);

        // Tapping the link shouldn't enable the radio button.
        var naturalScrollOn =
            touchpadPage.$$('paper-radio-button[name="true"]');
        var a = naturalScrollOn.querySelector('a');

        MockInteractions.tap(a);
        expectNaturalScrollValue(touchpadPage, false);

        MockInteractions.tap(naturalScrollOn);
        expectNaturalScrollValue(touchpadPage, true);

        devicePage.set('prefs.settings.touchpad.natural_scroll.value', false);
        expectNaturalScrollValue(touchpadPage, false);

        // Enter on the link shouldn't enable the radio button either.
        MockInteractions.pressEnter(a);

        // Annoyingly, we have to schedule an async event with a timeout greater
        // than or equal to the timeout used by IronButtonState (1).
        // https://github.com/PolymerElements/iron-behaviors/issues/54
        Polymer.Base.async(function() {
          expectNaturalScrollValue(touchpadPage, false);

          MockInteractions.pressEnter(naturalScrollOn);
          Polymer.Base.async(function() {
            expectNaturalScrollValue(touchpadPage, true);
            done();
          }, 1);
        }, 1);
      });
    });

    test(assert(TestNames.Keyboard), function() {
      // Open the keyboard subpage.
      return showAndGetDeviceSubpage('keyboard').then(function(keyboardPage) {
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

        var collapse = keyboardPage.$$('iron-collapse');
        assertTrue(!!collapse);
        expectTrue(collapse.opened);

        expectEquals(500, keyboardPage.$.delaySlider.value);
        expectEquals(500, keyboardPage.$.repeatRateSlider.value);

        // Test interaction with the cr-slider's underlying paper-slider.
        MockInteractions.pressAndReleaseKeyOn(
            keyboardPage.$.delaySlider.$.slider, 37 /* left */);
        MockInteractions.pressAndReleaseKeyOn(
            keyboardPage.$.repeatRateSlider.$.slider, 39 /* right */);
        expectEquals(
            1000, fakePrefs.settings.language.xkb_auto_repeat_delay_r2.value);
        expectEquals(
            300,
            fakePrefs.settings.language.xkb_auto_repeat_interval_r2.value);

        // Test sliders change when prefs change.
        devicePage.set(
            'prefs.settings.language.xkb_auto_repeat_delay_r2.value', 1500);
        expectEquals(1500, keyboardPage.$.delaySlider.value);
        devicePage.set(
            'prefs.settings.language.xkb_auto_repeat_interval_r2.value',
            2000);
        expectEquals(2000, keyboardPage.$.repeatRateSlider.value);

        // Test sliders round to nearest value when prefs change.
        devicePage.set(
            'prefs.settings.language.xkb_auto_repeat_delay_r2.value', 600);
        expectEquals(600, keyboardPage.$.delaySlider.value);
        devicePage.set(
            'prefs.settings.language.xkb_auto_repeat_interval_r2.value', 45);
        expectEquals(45, keyboardPage.$.repeatRateSlider.value);

        devicePage.set(
            'prefs.settings.language.xkb_auto_repeat_enabled_r2.value',
            false);
        expectFalse(collapse.opened);

        // Test keyboard shortcut overlay button.
        MockInteractions.tap(keyboardPage.$$('#keyboardOverlay'));
        expectEquals(
            1,
            settings.DevicePageBrowserProxyImpl.getInstance()
                .keyboardShortcutsOverlayShown_);
      });
    });

    test(assert(TestNames.Display), function() {
      // Open the display subpage.
      var displayPage = showAndGetDeviceSubpage('display');
      assertTrue(!!displayPage);

      var addDisplay = function(n) {
        var display = {
          id: 'fakeDisplayId' + n,
          name: 'fakeDisplayName' + n,
          mirroring: '',
          isPrimary: n == 1,
          rotation: 0,
          modes: [],
          bounds: {
            left: 0,
            top: 0,
            width: 1920,
            height: 1080,
          },
        };
        fakeSystemDisplay.addDisplayForTest(display);
      };

      var displayPage;
      return Promise.all([
        // Get the display sub-page.
        showAndGetDeviceSubpage('display').then(function(page) {
          displayPage = page;
        }),
        // Wait for the initial call to getInfo.
        fakeSystemDisplay.getInfoCalled.promise,
      ]).then(function() {
        // Add a display.
        addDisplay(1);
        fakeSystemDisplay.onDisplayChanged.callListeners();

        return Promise.all([
          fakeSystemDisplay.getInfoCalled.promise,
          fakeSystemDisplay.getLayoutCalled.promise,
          new Promise(function(resolve, reject) { setTimeout(resolve); })
        ]);
      }).then(function() {
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

        return Promise.all([
          fakeSystemDisplay.getInfoCalled.promise,
          fakeSystemDisplay.getLayoutCalled.promise,
          new Promise(function(resolve, reject) { setTimeout(resolve); })
        ]);
      }).then(function() {
        // There should be two displays, the first should be primary and
        // selected. Mirroring should be enabled but set to false.
        expectEquals(2, displayPage.displays.length);
        expectEquals(
            displayPage.displays[0].id, displayPage.selectedDisplay.id);
        expectEquals(displayPage.displays[0].id, displayPage.primaryDisplayId);
        expectTrue(displayPage.showMirror_(displayPage.displays));
        expectFalse(displayPage.isMirrored_(displayPage.displays));

        // Select the second display and make it primary. Also change the
        // orientation of the second display.
        var displayLayout = displayPage.$$('#displayLayout');
        assertTrue(!!displayLayout);
        var displayDiv = displayLayout.$$('#_fakeDisplayId2');
        assertTrue(!!displayDiv);
        MockInteractions.tap(displayDiv);
        expectEquals(
            displayPage.displays[1].id, displayPage.selectedDisplay.id);

        displayPage.onMakePrimaryTap_();
        displayPage.onSetOrientation_({detail: {selected: '90'}});
        fakeSystemDisplay.onDisplayChanged.callListeners();

        return Promise.all([
          fakeSystemDisplay.getInfoCalled.promise,
          fakeSystemDisplay.getLayoutCalled.promise,
          new Promise(function(resolve, reject) { setTimeout(resolve); })
        ]);
      }).then(function() {
        // Confirm that the second display is selected, primary, and rotated.
        expectEquals(2, displayPage.displays.length);
        expectEquals(
            displayPage.displays[1].id, displayPage.selectedDisplay.id);
        expectTrue(displayPage.displays[1].isPrimary);
        expectEquals(displayPage.displays[1].id, displayPage.primaryDisplayId);
        expectEquals(90, displayPage.displays[1].rotation);

        // Mirror the displays.
        displayPage.onMirroredTap_();
        fakeSystemDisplay.onDisplayChanged.callListeners();

        return Promise.all([
          fakeSystemDisplay.getInfoCalled.promise,
          fakeSystemDisplay.getLayoutCalled.promise,
          new Promise(function(resolve, reject) { setTimeout(resolve); })
        ]);
      }).then(function() {
        // Confirm that there is now only one display and that it is primary
        // and mirroring is enabled.
        expectEquals(1, displayPage.displays.length);
        expectEquals(
            displayPage.displays[0].id, displayPage.selectedDisplay.id);
        expectTrue(displayPage.displays[0].isPrimary);
        expectTrue(displayPage.showMirror_(displayPage.displays));
        expectTrue(displayPage.isMirrored_(displayPage.displays));
      });
    });
  });

  return {
    TestNames: TestNames
  };
});
