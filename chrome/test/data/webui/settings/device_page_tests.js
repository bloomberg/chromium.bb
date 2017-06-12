// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('device_page_tests', function() {
  /** @enum {string} */
  var TestNames = {
    DevicePage: 'device page',
    Display: 'display',
    Keyboard: 'keyboard',
    Pointers: 'pointers',
    Power: 'power',
    Stylus: 'stylus',
  };

  /**
   * @constructor
   * @implements {settings.DevicePageBrowserProxy}
   */
  function TestDevicePageBrowserProxy() {
    this.keyboardShortcutsOverlayShown_ = 0;
    this.updatePowerStatusCalled_ = 0;
    this.onNoteTakingAppsUpdated_ = null;
    this.requestNoteTakingApps_ = 0;
    this.setPreferredNoteTakingApp_ = '';
  }

  TestDevicePageBrowserProxy.prototype = {
    /** override */
    initializePointers: function() {
      // Enable mouse and touchpad.
      cr.webUIListenerCallback('has-mouse-changed', true);
      cr.webUIListenerCallback('has-touchpad-changed', true);
    },

    /** override */
    initializeStylus: function() {
      // Enable stylus.
      cr.webUIListenerCallback('has-stylus-changed', true);
    },

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

    /** @override */
    updatePowerStatus: function() {
      this.updatePowerStatusCalled_++;
    },

    /** @override */
    setPowerSource: function(powerSourceId) {
      this.powerSourceId_ = powerSourceId;
    },

    /** @override */
    setNoteTakingAppsUpdatedCallback: function(callback) {
      this.onNoteTakingAppsUpdated_ = callback;
    },

    /** @override */
    requestNoteTakingApps: function() {
      this.requestNoteTakingApps_++;
    },

    /** @override */
    setPreferredNoteTakingApp: function(appId) {
      this.setPreferredNoteTakingApp_ = appId;
    },
  };

  function getFakePrefs() {
    return {
      ash: {
        night_light: {
          schedule_type: {
            key: 'ash.night_light.schedule_type',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
        },
      },
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
            value: false,
          },
          natural_scroll: {
            key: 'settings.touchpad.natural_scroll',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          sensitivity2: {
            key: 'settings.touchpad.sensitivity2',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 3,
          },
        },
        mouse: {
          primary_right: {
            key: 'settings.mouse.primary_right',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          sensitivity2: {
            key: 'settings.mouse.sensitivity2',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 4,
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
          remap_escape_key_to: {
            key: 'settings.language.remap_escape_key_to',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 5,
          },
          remap_backspace_key_to: {
            key: 'settings.language.remap_backspace_key_to',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 6,
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
        },
        note_taking_app_enabled_on_lock_screen: {
          key: 'settings.note_taking_app_enabled_on_lock_screen',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false
        }
      }
    };
  }

  suite('SettingsDevicePage', function() {
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
      settings.navigateTo(settings.Route.BASIC);

      devicePage = document.createElement('settings-device-page');
      devicePage.prefs = getFakePrefs();
      settings.DevicePageBrowserProxyImpl.instance_ =
          new TestDevicePageBrowserProxy();

      // settings-animated-pages expects a parent with data-page set.
      var basicPage = document.createElement('div');
      basicPage.dataset.page = 'basic';
      basicPage.appendChild(devicePage);
      document.body.appendChild(basicPage);

      // Allow the light DOM to be distributed to settings-animated-pages.
      setTimeout(done);
    });

    /** @return {!Promise<!HTMLElement>} */
    function showAndGetDeviceSubpage(subpage, expectedRoute) {
      return new Promise(function(resolve, reject) {
        var row = assert(devicePage.$$('#main #' + subpage + 'Row'));
        devicePage.$$('#pages').addEventListener(
            'neon-animation-finish', resolve);
        MockInteractions.tap(row);
      }).then(function() {
        assertEquals(expectedRoute, settings.getCurrentRoute());
        var page = devicePage.$$('settings-' + subpage);
        return assert(page);
      });
    };

    /**
     * @param {!HTMLElement} pointersPage
     * @param {boolean} expected
     */
    function expectNaturalScrollValue(pointersPage, expected) {
      var naturalScrollOff =
          pointersPage.$$('paper-radio-button[name="false"]');
      var naturalScrollOn =
          pointersPage.$$('paper-radio-button[name="true"]');
      assertTrue(!!naturalScrollOff);
      assertTrue(!!naturalScrollOn);

      expectEquals(!expected, naturalScrollOff.checked);
      expectEquals(expected, naturalScrollOn.checked);
      expectEquals(expected,
                   devicePage.prefs.settings.touchpad.natural_scroll.value);
    }

    test(assert(TestNames.DevicePage), function() {
      expectLT(0, devicePage.$$('#pointersRow').offsetHeight);
      expectLT(0, devicePage.$$('#keyboardRow').offsetHeight);
      expectLT(0, devicePage.$$('#displayRow').offsetHeight);

      cr.webUIListenerCallback('has-mouse-changed', false);
      expectLT(0, devicePage.$$('#pointersRow').offsetHeight);
      cr.webUIListenerCallback('has-touchpad-changed', false);
      expectEquals(0, devicePage.$$('#pointersRow').offsetHeight);
      cr.webUIListenerCallback('has-mouse-changed', true);
      expectLT(0, devicePage.$$('#pointersRow').offsetHeight);
    });

    suite(assert(TestNames.Pointers), function() {
      var pointersPage;

      setup(function() {
        return showAndGetDeviceSubpage(
            'pointers', settings.Route.POINTERS).then(function(page) {
              pointersPage = page;
            });
      });

      test('subpage responds to pointer attach/detach', function() {
        assertEquals(settings.Route.POINTERS, settings.getCurrentRoute());
        assertLT(0, pointersPage.$$('#mouse').offsetHeight);
        assertLT(0, pointersPage.$$('#touchpad').offsetHeight);
        assertLT(0, pointersPage.$$('#mouse h2').offsetHeight);
        assertLT(0, pointersPage.$$('#touchpad h2').offsetHeight);

        cr.webUIListenerCallback('has-touchpad-changed', false);
        assertEquals(settings.Route.POINTERS, settings.getCurrentRoute());
        assertLT(0, pointersPage.$$('#mouse').offsetHeight);
        assertEquals(0, pointersPage.$$('#touchpad').offsetHeight);
        assertEquals(0, pointersPage.$$('#mouse h2').offsetHeight);
        assertEquals(0, pointersPage.$$('#touchpad h2').offsetHeight);

        // Wait for the transition back to the main page.
        return new Promise(function(resolve, reject) {
          devicePage.$$('#pages').addEventListener(
              'neon-animation-finish', resolve);

          cr.webUIListenerCallback('has-mouse-changed', false);
        }).then(function() {
          assertEquals(settings.Route.DEVICE, settings.getCurrentRoute());
          assertEquals(0, devicePage.$$('#main #pointersRow').offsetHeight);

          cr.webUIListenerCallback('has-touchpad-changed', true);
          assertLT(0, devicePage.$$('#main #pointersRow').offsetHeight);
          return showAndGetDeviceSubpage('pointers', settings.Route.POINTERS);
        }).then(function(page) {
          assertEquals(0, pointersPage.$$('#mouse').offsetHeight);
          assertLT(0, pointersPage.$$('#touchpad').offsetHeight);
          assertEquals(0, pointersPage.$$('#mouse h2').offsetHeight);
          assertEquals(0, pointersPage.$$('#touchpad h2').offsetHeight);

          cr.webUIListenerCallback('has-mouse-changed', true);
          assertEquals(settings.Route.POINTERS, settings.getCurrentRoute());
          assertLT(0, pointersPage.$$('#mouse').offsetHeight);
          assertLT(0, pointersPage.$$('#touchpad').offsetHeight);
          assertLT(0, pointersPage.$$('#mouse h2').offsetHeight);
          assertLT(0, pointersPage.$$('#touchpad h2').offsetHeight);
        });
      });

      test('mouse', function() {
        expectLT(0, pointersPage.$$('#mouse').offsetHeight);

        expectFalse(pointersPage.$$('#mouse settings-toggle-button').checked);

        var slider = assert(pointersPage.$$('#mouse settings-slider'));
        expectEquals(4, slider.pref.value);
        MockInteractions.pressAndReleaseKeyOn(
            slider.$$('#slider'), 37 /* left */);
        expectEquals(3, devicePage.prefs.settings.mouse.sensitivity2.value);

        pointersPage.set('prefs.settings.mouse.sensitivity2.value', 5);
        expectEquals(5, slider.pref.value);
      });

      test('touchpad', function() {
        expectLT(0, pointersPage.$$('#touchpad').offsetHeight);

        expectTrue(pointersPage.$$('#touchpad #enableTapToClick').checked);
        expectFalse(pointersPage.$$('#touchpad #enableTapDragging').checked);

        var slider = assert(pointersPage.$$('#touchpad settings-slider'));
        expectEquals(3, slider.pref.value);
        MockInteractions.pressAndReleaseKeyOn(
            slider.$$('#slider'), 39 /* right */);
        expectEquals(4, devicePage.prefs.settings.touchpad.sensitivity2.value);

        pointersPage.set('prefs.settings.touchpad.sensitivity2.value', 2);
        expectEquals(2, slider.pref.value);
      });

      test('link doesn\'t activate control', function(done) {
        expectNaturalScrollValue(pointersPage, false);

        // Tapping the link shouldn't enable the radio button.
        var naturalScrollOn =
            pointersPage.$$('paper-radio-button[name="true"]');
        var a = naturalScrollOn.querySelector('a');

        MockInteractions.tap(a);
        expectNaturalScrollValue(pointersPage, false);

        MockInteractions.tap(naturalScrollOn);
        expectNaturalScrollValue(pointersPage, true);
        devicePage.set('prefs.settings.touchpad.natural_scroll.value', false);
        expectNaturalScrollValue(pointersPage, false);

        // Enter on the link shouldn't enable the radio button either.
        MockInteractions.pressEnter(a);

        // Annoyingly, we have to schedule an async event with a timeout
        // greater than or equal to the timeout used by IronButtonState (1).
        // https://github.com/PolymerElements/iron-behaviors/issues/54
        Polymer.Base.async(function() {
          expectNaturalScrollValue(pointersPage, false);

          MockInteractions.pressEnter(naturalScrollOn);
          Polymer.Base.async(function() {
            expectNaturalScrollValue(pointersPage, true);
            done();
          }, 1);
        }, 1);
      });
    });

    test(assert(TestNames.Keyboard), function() {
      // Open the keyboard subpage.
      return showAndGetDeviceSubpage(
          'keyboard', settings.Route.KEYBOARD).then(function(keyboardPage) {
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

        expectEquals(500, keyboardPage.$$('#delaySlider').pref.value);
        expectEquals(500, keyboardPage.$$('#repeatRateSlider').pref.value);

        // Test interaction with the settings-slider's underlying paper-slider.
        MockInteractions.pressAndReleaseKeyOn(
            keyboardPage.$$('#delaySlider').$$('#slider'), 37 /* left */);
        MockInteractions.pressAndReleaseKeyOn(
            keyboardPage.$$('#repeatRateSlider').$$('#slider'), 39 /* right */);
        var language = devicePage.prefs.settings.language;
        expectEquals(1000, language.xkb_auto_repeat_delay_r2.value);
        expectEquals(300, language.xkb_auto_repeat_interval_r2.value);

        // Test sliders change when prefs change.
        devicePage.set(
            'prefs.settings.language.xkb_auto_repeat_delay_r2.value', 1500);
        expectEquals(1500, keyboardPage.$$('#delaySlider').pref.value);
        devicePage.set(
            'prefs.settings.language.xkb_auto_repeat_interval_r2.value',
            2000);
        expectEquals(2000, keyboardPage.$$('#repeatRateSlider').pref.value);

        // Test sliders round to nearest value when prefs change.
        devicePage.set(
            'prefs.settings.language.xkb_auto_repeat_delay_r2.value', 600);
        expectEquals(500, keyboardPage.$$('#delaySlider').pref.value);
        devicePage.set(
            'prefs.settings.language.xkb_auto_repeat_interval_r2.value', 45);
        expectEquals(50, keyboardPage.$$('#repeatRateSlider').pref.value);

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
        showAndGetDeviceSubpage(
            'display', settings.Route.DISPLAY).then(function(page) {
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
        ]);
      }).then(function() {
        // There should be a single display which should be primary and
        // selected. Mirroring should be disabled.
        expectEquals(1, displayPage.displays.length);
        expectEquals(
            displayPage.displays[0].id, displayPage.selectedDisplay.id);
        expectEquals(
            displayPage.displays[0].id, displayPage.primaryDisplayId);
        expectFalse(displayPage.showMirror_(false, displayPage.displays));
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
        expectTrue(displayPage.showMirror_(false, displayPage.displays));
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

        displayPage.updatePrimaryDisplay_({target: {value: '0'}});
        displayPage.onOrientationChange_({target: {value: '90'}});
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
        expectTrue(displayPage.showMirror_(false, displayPage.displays));
        expectTrue(displayPage.isMirrored_(displayPage.displays));
      });
    });

    suite(assert(TestNames.Power), function() {
      /**
       * Sets power sources using a deep copy of |sources|.
       * @param {Array<settings.PowerSource>} sources
       * @param {string} powerSourceId
       * @param {bool} isLowPowerCharger
       */
      function setPowerSources(sources, powerSourceId, isLowPowerCharger) {
        var sourcesCopy = sources.map(function(source) {
          return Object.assign({}, source);
        });
        cr.webUIListenerCallback('power-sources-changed',
            sourcesCopy, powerSourceId, isLowPowerCharger);
      }

      suite('no power settings', function() {
        test('power row hidden', function() {
          assertEquals(null, devicePage.$$('#powerRow'));
          assertEquals(0,
              settings.DevicePageBrowserProxyImpl.getInstance()
              .updatePowerStatusCalled_);
        });
      });

      suite('power settings', function() {
        var powerPage;
        var powerSourceRow;
        var powerSourceWrapper;
        var powerSourceSelect;

        suiteSetup(function() {
          // Always show power settings.
          loadTimeData.overrideValues({
            enablePowerSettings: true,
          });
        });

        setup(function() {
          return showAndGetDeviceSubpage('power', settings.Route.POWER)
              .then(function(page) {
                powerPage = page;
                powerSourceRow = assert(powerPage.$$('#powerSourceRow'));
                powerSourceWrapper =
                    assert(powerSourceRow.querySelector('.md-select-wrapper'));
                powerSourceSelect = assert(powerPage.$$('#powerSource'));
                assertEquals(
                    1,
                    settings.DevicePageBrowserProxyImpl.getInstance()
                        .updatePowerStatusCalled_);
              });
        });

        test('power sources', function() {
          var batteryStatus = {
            charging: false,
            calculating: false,
            percent: 50,
            statusText: '5 hours left',
          };
          cr.webUIListenerCallback(
              'battery-status-changed', Object.assign({}, batteryStatus));
          setPowerSources([], '', false);
          Polymer.dom.flush();

          // Power sources dropdown is hidden.
          assertTrue(powerSourceWrapper.hidden);

          // Attach a dual-role USB device.
          var powerSource = {
            id: '2',
            type: settings.PowerDeviceType.DUAL_ROLE_USB,
            description: 'USB-C device',
          };
          setPowerSources([powerSource], '', false);
          Polymer.dom.flush();

          // "Battery" should be selected.
          assertFalse(powerSourceWrapper.hidden);
          assertEquals('', powerSourceSelect.value);

          // Select the power source.
          setPowerSources([powerSource], powerSource.id, true);
          Polymer.dom.flush();
          assertFalse(powerSourceWrapper.hidden);
          assertEquals(powerSource.id, powerSourceSelect.value);

          // Send another power source; the first should still be selected.
          var otherPowerSource = Object.assign({}, powerSource);
          otherPowerSource.id = '3';
          setPowerSources(
              [otherPowerSource, powerSource], powerSource.id, true);
          Polymer.dom.flush();
          assertFalse(powerSourceWrapper.hidden);
          assertEquals(powerSource.id, powerSourceSelect.value);
        });

        test('choose power source', function() {
          var batteryStatus = {
            charging: false,
            calculating: false,
            percent: 50,
            statusText: '5 hours left',
          };
          cr.webUIListenerCallback(
              'battery-status-changed', Object.assign({}, batteryStatus));

          // Attach a dual-role USB device.
          var powerSource = {
            id: '3',
            type: settings.PowerDeviceType.DUAL_ROLE_USB,
            description: 'USB-C device',
          };
          setPowerSources([powerSource], '', false);
          Polymer.dom.flush();

          // Select the device.
          powerSourceSelect.value = powerSourceSelect.children[1].value;
          powerSourceSelect.dispatchEvent(new CustomEvent('change'));
          Polymer.dom.flush();
          expectEquals(
              powerSource.id,
              settings.DevicePageBrowserProxyImpl.getInstance().powerSourceId_);
        });
      });
    });

    suite(assert(TestNames.Stylus), function() {
      var stylusPage;
      var appSelector;
      var browserProxy;
      var noAppsDiv;
      var waitingDiv;
      var selectAppDiv;


      suiteSetup(function() {
        // Always show stylus settings.
        loadTimeData.overrideValues({
          stylusAllowed: true,
        });
      });

      setup(function() {
        return showAndGetDeviceSubpage('stylus', settings.Route.STYLUS).then(
            function(page) {
              stylusPage = page;
              browserProxy = settings.DevicePageBrowserProxyImpl.getInstance();
              appSelector = assert(page.$$('#menu'));
              noAppsDiv = assert(page.$$('#no-apps'));
              waitingDiv = assert(page.$$('#waiting'));
              selectAppDiv = assert(page.$$('#select-app'));

              assertEquals(1, browserProxy.requestNoteTakingApps_);
              assertEquals('', browserProxy.setPreferredNoteTakingApp_);
              assert(browserProxy.onNoteTakingAppsUpdated_);
            });
      });

      // Helper function to allocate a note app entry.
      function entry(name, value, preferred, supportsLockScreen) {
        return {
          name: name,
          value: value,
          preferred: preferred,
          supportsLockScreen: supportsLockScreen
        };
      }

      /**
       * @return {Element | undefined}
       */
      function enableAppOnLockScreenToggle() {
        return stylusPage.$$('#enable-app-on-lock-screen-toggle');
      }

      /**
       * @param {Element|undefined} element
       */
      function isVisible(element) {
        return !!element && element.offsetWidth > 0 && element.offsetHeight > 0;
      }

      test('initial app choice selector value', function() {
        // Selector chooses the first value in list if there is no preferred
        // value set.
        browserProxy.onNoteTakingAppsUpdated_([
          entry('n1', 'v1', false, false),
          entry('n2', 'v2', false, false)
        ], false);
        Polymer.dom.flush();
        assertEquals('v1', appSelector.value);

        // Selector chooses the preferred value if set.
        browserProxy.onNoteTakingAppsUpdated_([
          entry('n1', 'v1', false, false),
          entry('n2', 'v2', true, false)
        ], false);
        Polymer.dom.flush();
        assertEquals('v2', appSelector.value);
      });

      test('change preferred app', function() {
        // Load app list.
        browserProxy.onNoteTakingAppsUpdated_([
          entry('n1', 'v1', false, false),
          entry('n2', 'v2', true, false)
        ], false);
        Polymer.dom.flush();
        assertEquals('', browserProxy.setPreferredNoteTakingApp_);

        // Update select element to new value, verify browser proxy is called.
        appSelector.value = 'v1';
        stylusPage.onSelectedAppChanged_();
        assertEquals('v1', browserProxy.setPreferredNoteTakingApp_);
      });

      test('preferred app does not change without interaction', function() {
        // Pass various types of data to page, verify the preferred note-app
        // does not change.
        browserProxy.onNoteTakingAppsUpdated_([], false);
        Polymer.dom.flush();
        assertEquals('', browserProxy.setPreferredNoteTakingApp_);

        browserProxy.onNoteTakingAppsUpdated_([], true);
        Polymer.dom.flush();
        assertEquals('', browserProxy.setPreferredNoteTakingApp_);

        browserProxy.onNoteTakingAppsUpdated_([
          entry('n', 'v', false, false)
        ], true);
        Polymer.dom.flush();
        assertEquals('', browserProxy.setPreferredNoteTakingApp_);

        browserProxy.onNoteTakingAppsUpdated_([
          entry('n', 'v', false, false)
        ], false);
        Polymer.dom.flush();
        assertEquals('', browserProxy.setPreferredNoteTakingApp_);

        browserProxy.onNoteTakingAppsUpdated_([
          entry('n1', 'v1', false, false),
          entry('n2', 'v2', true, false)
        ], false);
        Polymer.dom.flush();
        assertEquals('', browserProxy.setPreferredNoteTakingApp_);
      });

      test('app-visibility', function() {
        // No apps available.
        browserProxy.onNoteTakingAppsUpdated_([], false);
        assert(!noAppsDiv.hidden);
        assert(waitingDiv.hidden);
        assert(selectAppDiv.hidden);

        // Waiting for apps to finish loading.
        browserProxy.onNoteTakingAppsUpdated_([], true);
        assert(noAppsDiv.hidden);
        assert(!waitingDiv.hidden);
        assert(selectAppDiv.hidden);

        browserProxy.onNoteTakingAppsUpdated_([
          entry('n', 'v', false, false)
        ], true);
        assert(noAppsDiv.hidden);
        assert(!waitingDiv.hidden);
        assert(selectAppDiv.hidden);

        // Apps loaded, show selector.
        browserProxy.onNoteTakingAppsUpdated_([
          entry('n', 'v', false, false)
        ], false);
        assert(noAppsDiv.hidden);
        assert(waitingDiv.hidden);
        assert(!selectAppDiv.hidden);
      });

      test('enabled-on-lock-screen', function() {
        expectFalse(isVisible(enableAppOnLockScreenToggle()));

        return new Promise(function(resolve) {
          // No apps available.
          browserProxy.onNoteTakingAppsUpdated_([], false);
          stylusPage.async(resolve);
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(enableAppOnLockScreenToggle()));

          // Single app which does not support lock screen note taking.
          browserProxy.onNoteTakingAppsUpdated_([
            entry('n1', 'v1', false, false)
          ], false);
          return new Promise(function(resolve) {stylusPage.async(resolve);});
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(enableAppOnLockScreenToggle()));

          // Add an app with lock screen support, but do not select it yet.
          browserProxy.onNoteTakingAppsUpdated_([
            entry('n1', 'v1', false, false),
            entry('n2', 'v2', false, true)
          ], false);
          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(enableAppOnLockScreenToggle()));

          // Select the app with lock screen app support.
          appSelector.value = 'v2';
          stylusPage.onSelectedAppChanged_();
          assertEquals('v2', browserProxy.setPreferredNoteTakingApp_);

          Polymer.dom.flush();
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);

          devicePage.set(
              'prefs.settings.note_taking_app_enabled_on_lock_screen.value',
              true);
          Polymer.dom.flush();
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectTrue(enableAppOnLockScreenToggle().checked);

          // Select the app that does not support lock screen again.
          appSelector.value = 'v1';
          stylusPage.onSelectedAppChanged_();
          assertEquals('v1', browserProxy.setPreferredNoteTakingApp_);

          Polymer.dom.flush();
          expectFalse(isVisible(enableAppOnLockScreenToggle()));
        });
      });

      test('initial-app-lock-screen-enabled', function() {
        return new Promise(function(resolve) {
          // No apps available.
          browserProxy.onNoteTakingAppsUpdated_([
            entry('n1', 'v1', true, true)
          ], false);
          stylusPage.async(resolve);
        }).then(function() {
          Polymer.dom.flush();

          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);

          devicePage.set(
              'prefs.settings.note_taking_app_enabled_on_lock_screen.value',
              true);
          Polymer.dom.flush();
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectTrue(enableAppOnLockScreenToggle().checked);

          devicePage.set(
              'prefs.settings.note_taking_app_enabled_on_lock_screen.value',
              false);
          Polymer.dom.flush();
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);

          browserProxy.onNoteTakingAppsUpdated_([], false);
          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(enableAppOnLockScreenToggle()));
        });
      });

      test('tap-on-enable-note-taking-on-lock-screen', function() {
        return new Promise(function(resolve) {
          // No apps available.
          browserProxy.onNoteTakingAppsUpdated_([
            entry('n1', 'v1', true, true)
          ], false);
          stylusPage.async(resolve);
        }).then(function() {
          Polymer.dom.flush();

          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);

          expectFalse(
              devicePage.prefs.settings.note_taking_app_enabled_on_lock_screen
                  .value);
          Polymer.dom.flush();
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);

          MockInteractions.tap(enableAppOnLockScreenToggle().$$('#control'));
          expectTrue(enableAppOnLockScreenToggle().checked);
          expectTrue(
              devicePage.prefs.settings.note_taking_app_enabled_on_lock_screen
                  .value);

          MockInteractions.tap(enableAppOnLockScreenToggle().$$('#control'));
          expectFalse(enableAppOnLockScreenToggle().checked);
          expectFalse(
              devicePage.prefs.settings.note_taking_app_enabled_on_lock_screen
                  .value);
        });
      });
    });
  });

  return {
    TestNames: TestNames
  };
});
