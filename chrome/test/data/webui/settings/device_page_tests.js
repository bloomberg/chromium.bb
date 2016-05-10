// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_device_page', function() {
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
            key: 'settings.language.xkb_auto_repeat_delay_r2',
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

    setup(function() {
      fakeSystemDisplay = new settings.FakeSystemDisplay();
      settings.display.systemDisplayApi = fakeSystemDisplay;

      PolymerTest.clearBody();
      devicePage = document.createElement('settings-device-page');
      devicePage.currentRoute = {page: 'basic', section: '', subpage: []};
      devicePage.prefs = fakePrefs;
      settings.DevicePageBrowserProxyImpl.instance_ =
          new TestDevicePageBrowserProxy();
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

    test('touchpad subpage', function(done) {
      // Open the touchpad subpage.
      var touchpadPage = showAndGetDeviceSubpage('touchpad');
      assertTrue(!!touchpadPage);

      expectNaturalScrollValue(touchpadPage, false);

      // Tapping the link shouldn't enable the radio button.
      var naturalScrollOn = touchpadPage.$$('paper-radio-button[name="true"]');
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

      var collapse = keyboardPage.$$('iron-collapse');
      assertTrue(!!collapse);
      expectTrue(collapse.opened);

      // Values are based on indices of auto-repeat options in keyboard.js.
      expectEquals(keyboardPage.$.delaySlider.immediateValue, 3);
      expectEquals(keyboardPage.$.repeatRateSlider.immediateValue, 2);

      // Test interaction with slider.
      MockInteractions.pressAndReleaseKeyOn(
          keyboardPage.$.delaySlider, 37 /* left */);
      MockInteractions.pressAndReleaseKeyOn(
          keyboardPage.$.repeatRateSlider, 39 /* right */);
      expectEquals(
          fakePrefs.settings.language.xkb_auto_repeat_delay_r2.value, 1000);
      expectEquals(
          fakePrefs.settings.language.xkb_auto_repeat_interval_r2.value,
          300);

      // Test sliders change when prefs change.
      devicePage.set(
          'prefs.settings.language.xkb_auto_repeat_delay_r2.value', 1500);
      expectEquals(keyboardPage.$.delaySlider.immediateValue, 1);
      devicePage.set(
          'prefs.settings.language.xkb_auto_repeat_interval_r2.value', 2000);
      expectEquals(keyboardPage.$.repeatRateSlider.immediateValue, 0);

      // Test sliders round to nearest value when prefs change.
      devicePage.set(
          'prefs.settings.language.xkb_auto_repeat_delay_r2.value', 600);
      expectEquals(keyboardPage.$.delaySlider.immediateValue, 3 /* 500 */);
      devicePage.set(
          'prefs.settings.language.xkb_auto_repeat_interval_r2.value', 45);
      expectEquals(keyboardPage.$.repeatRateSlider.immediateValue, 6 /* 50 */);

      devicePage.set(
          'prefs.settings.language.xkb_auto_repeat_enabled_r2.value', false);
      expectFalse(collapse.opened);

      // Test keyboard shortcut overlay button.
      MockInteractions.tap(keyboardPage.$$('#keyboardOverlay'));
      expectEquals(
          1,
          settings.DevicePageBrowserProxyImpl.getInstance()
              .keyboardShortcutsOverlayShown_);
    });

    // Test more edge cases for slider rounding logic.
    // TODO(michaelpg): Move this test to settings-slider tests once that
    // element is created.
    test('keyboard sliders', function() {
      var keyboardPage = showAndGetDeviceSubpage('keyboard');
      assertTrue(!!keyboardPage);

      var testArray = [80, 20, 350, 1000, 200, 100];
      var testFindNearestIndex = function(expectedIndex, value) {
        expectEquals(
            expectedIndex, keyboardPage.findNearestIndex_(testArray, value));
      };
      testFindNearestIndex(0, 51);
      testFindNearestIndex(0, 80);
      testFindNearestIndex(0, 89);
      testFindNearestIndex(1, -100);
      testFindNearestIndex(1, 20);
      testFindNearestIndex(1, 49);
      testFindNearestIndex(2, 400);
      testFindNearestIndex(2, 350);
      testFindNearestIndex(2, 300);
      testFindNearestIndex(3, 200000);
      testFindNearestIndex(3, 1000);
      testFindNearestIndex(3, 700);
      testFindNearestIndex(4, 220);
      testFindNearestIndex(4, 200);
      testFindNearestIndex(4, 151);
      testFindNearestIndex(5, 149);
      testFindNearestIndex(5, 100);
      testFindNearestIndex(5, 91);
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
