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
    this.requestPowerManagementSettingsCalled_ = 0;
    this.requestNoteTakingApps_ = 0;
    this.onNoteTakingAppsUpdated_ = null;

    this.androidAppsReceived_ = false;
    this.noteTakingApps_ = [];
    this.setPreferredAppCount_ = 0;
    this.setAppOnLockScreenCount_ = 0;
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
    requestPowerManagementSettings: function() {
      this.requestPowerManagementSettingsCalled_++;
    },

    /** @override */
    setIdleBehavior: function(behavior) {
      this.idleBehavior_ = behavior;
    },

    /** @override */
    setLidClosedBehavior: function(behavior) {
      this.lidClosedBehavior_ = behavior;
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
      ++this.setPreferredAppCount_;

      var changed = false;
      this.noteTakingApps_.forEach(function(app) {
        changed = changed || app.preferred != (app.value == appId);
        app.preferred = app.value == appId;
      });

      if (changed)
        this.scheduleLockScreenAppsUpdated_();
    },

    /** @override */
    setPreferredNoteTakingAppEnabledOnLockScreen: function(enabled) {
      ++this.setAppOnLockScreenCount_;

      this.noteTakingApps_.forEach(function(app) {
        if (enabled) {
          if (app.preferred) {
            assertEquals(settings.NoteAppLockScreenSupport.SUPPORTED,
                         app.lockScreenSupport);
          }
          if (app.lockScreenSupport ==
                  settings.NoteAppLockScreenSupport.SUPPORTED) {
            app.lockScreenSupport = settings.NoteAppLockScreenSupport.ENABLED;
          }
        } else {
          if (app.preferred) {
            assertEquals(settings.NoteAppLockScreenSupport.ENABLED,
                         app.lockScreenSupport);
          }
          if (app.lockScreenSupport ==
                  settings.NoteAppLockScreenSupport.ENABLED) {
            app.lockScreenSupport = settings.NoteAppLockScreenSupport.SUPPORTED;
          }
        }
      });

      this.scheduleLockScreenAppsUpdated_();
    },

    // Test interface:
    /**
     * Sets whether the app list contains Android apps.
     * @param {boolean} Whether the list of Android note-taking apps was
     *     received.
     */
    setAndroidAppsReceived: function(received) {
      this.androidAppsReceived_ = received;

      this.scheduleLockScreenAppsUpdated_();
    },

    /**
     * @return {string} App id of the app currently selected as preferred.
     */
    getPreferredNoteTakingAppId: function() {
      var app = this.noteTakingApps_.find(function(existing) {
        return existing.preferred;
      });

      return app ? app.value : '';
    },

    /**
     * @return {settings.NoteAppLockScreenSupport | undefined} The lock screen
     *     support state of the app currently selected as preferred.
     */
    getPreferredAppLockScreenState: function() {
      var app = this.noteTakingApps_.find(function(existing) {
        return existing.preferred;
      });

      return app ? app.lockScreenSupport : undefined;
    },

    /**
     * Sets the current list of known note taking apps.
     * @param {Array<!settings.NoteAppInfo>} The list of apps to set.
     */
    setNoteTakingApps: function(apps) {
      this.noteTakingApps_ = apps;
      this.scheduleLockScreenAppsUpdated_();
    },

    /**
     * Adds an app to the list of known note-taking apps.
     * @param {!settings.NoteAppInfo}
     */
    addNoteTakingApp: function(app) {
      assert(!this.noteTakingApps_.find(function(existing) {
        return existing.value === app.value;
      }));

      this.noteTakingApps_.push(app);
      this.scheduleLockScreenAppsUpdated_();
    },

    /**
     * Invokes the registered note taking apps update callback.
     * @private
     */
    scheduleLockScreenAppsUpdated_: function() {
      this.onNoteTakingAppsUpdated_(this.noteTakingApps_.map(function(app) {
        return Object.assign({}, app);
      }), !this.androidAppsReceived_);
    }
  };

  function getFakePrefs() {
    return {
      ash: {
        night_light: {
          enabled: {
            key: 'ash.night_light.enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
          color_temperature: {
            key: 'ash.night_light.color_temperature',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
          schedule_type: {
            key: 'ash.night_light.schedule_type',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
          custom_start_time: {
            key: 'ash.night_light.custom_start_time',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
          custom_end_time: {
            key: 'ash.night_light.custom_end_time',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          },
        },
      },
      settings: {
        // TODO(afakhry): Write tests to validate the Night Light slider
        // behavior with 24-hour setting.
        clock: {
          use_24hour_clock: {
            key: 'settings.clock.use_24hour_clock',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
        restore_last_lock_screen_note: {
          key: 'settings.restore_last_lock_screen_note',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
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
      settings.navigateTo(settings.routes.BASIC);

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
     * @param {settings.IdleBehavior} idleBehavior
     * @param {boolean} idleControlled
     * @param {settings.LidClosedBehavior} lidClosedBehavior
     * @param {boolean} lidClosedControlled
     * @param {boolean} hasLid
     */
    function sendPowerManagementSettings(idleBehavior, idleControlled,
                                         lidClosedBehavior, lidClosedControlled,
                                         hasLid) {
      cr.webUIListenerCallback(
          'power-management-settings-changed',
          {
            idleBehavior: idleBehavior,
            idleControlled: idleControlled,
            lidClosedBehavior: lidClosedBehavior,
            lidClosedControlled: lidClosedControlled,
            hasLid: hasLid,
          });
      Polymer.dom.flush();
    };

    /**
     * @param {!HTMLElement} select
     * @param {!value} string
     */
    function selectValue(select, value) {
      select.value = value;
      select.dispatchEvent(new CustomEvent('change'));
      Polymer.dom.flush();
    }

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
        return showAndGetDeviceSubpage('pointers', settings.routes.POINTERS)
            .then(function(page) {
              pointersPage = page;
            });
      });

      test('subpage responds to pointer attach/detach', function() {
        assertEquals(settings.routes.POINTERS, settings.getCurrentRoute());
        assertLT(0, pointersPage.$$('#mouse').offsetHeight);
        assertLT(0, pointersPage.$$('#touchpad').offsetHeight);
        assertLT(0, pointersPage.$$('#mouse h2').offsetHeight);
        assertLT(0, pointersPage.$$('#touchpad h2').offsetHeight);

        cr.webUIListenerCallback('has-touchpad-changed', false);
        assertEquals(settings.routes.POINTERS, settings.getCurrentRoute());
        assertLT(0, pointersPage.$$('#mouse').offsetHeight);
        assertEquals(0, pointersPage.$$('#touchpad').offsetHeight);
        assertEquals(0, pointersPage.$$('#mouse h2').offsetHeight);
        assertEquals(0, pointersPage.$$('#touchpad h2').offsetHeight);

        // Wait for the transition back to the main page.
        return new Promise(function(resolve, reject) {
                 devicePage.$$('#pages').addEventListener(
                     'neon-animation-finish', resolve);

                 cr.webUIListenerCallback('has-mouse-changed', false);
               })
            .then(function() {
              assertEquals(settings.routes.DEVICE, settings.getCurrentRoute());
              assertEquals(0, devicePage.$$('#main #pointersRow').offsetHeight);

              cr.webUIListenerCallback('has-touchpad-changed', true);
              assertLT(0, devicePage.$$('#main #pointersRow').offsetHeight);
              return showAndGetDeviceSubpage(
                  'pointers', settings.routes.POINTERS);
            })
            .then(function(page) {
              assertEquals(0, pointersPage.$$('#mouse').offsetHeight);
              assertLT(0, pointersPage.$$('#touchpad').offsetHeight);
              assertEquals(0, pointersPage.$$('#mouse h2').offsetHeight);
              assertEquals(0, pointersPage.$$('#touchpad h2').offsetHeight);

              cr.webUIListenerCallback('has-mouse-changed', true);
              assertEquals(
                  settings.routes.POINTERS, settings.getCurrentRoute());
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
      return showAndGetDeviceSubpage('keyboard', settings.routes.KEYBOARD)
          .then(function(keyboardPage) {
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

            // Test interaction with the settings-slider's underlying
            // paper-slider.
            MockInteractions.pressAndReleaseKeyOn(
                keyboardPage.$$('#delaySlider').$$('#slider'), 37 /* left */);
            MockInteractions.pressAndReleaseKeyOn(
                keyboardPage.$$('#repeatRateSlider').$$('#slider'),
                39 /* right */);
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
                'prefs.settings.language.xkb_auto_repeat_interval_r2.value',
                45);
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
      return Promise
          .all([
            // Get the display sub-page.
            showAndGetDeviceSubpage('display', settings.routes.DISPLAY)
                .then(function(page) {
                  displayPage = page;
                }),
            // Wait for the initial call to getInfo.
            fakeSystemDisplay.getInfoCalled.promise,
          ])
          .then(function() {
            // Add a display.
            addDisplay(1);
            fakeSystemDisplay.onDisplayChanged.callListeners();

            return Promise.all([
              fakeSystemDisplay.getInfoCalled.promise,
              fakeSystemDisplay.getLayoutCalled.promise,
            ]);
          })
          .then(function() {
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
              new Promise(function(resolve, reject) {
                setTimeout(resolve);
              })
            ]);
          })
          .then(function() {
            // There should be two displays, the first should be primary and
            // selected. Mirroring should be enabled but set to false.
            expectEquals(2, displayPage.displays.length);
            expectEquals(
                displayPage.displays[0].id, displayPage.selectedDisplay.id);
            expectEquals(
                displayPage.displays[0].id, displayPage.primaryDisplayId);
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
              new Promise(function(resolve, reject) {
                setTimeout(resolve);
              })
            ]);
          })
          .then(function() {
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

            return Promise.all([
              fakeSystemDisplay.getInfoCalled.promise,
              fakeSystemDisplay.getLayoutCalled.promise,
              new Promise(function(resolve, reject) {
                setTimeout(resolve);
              })
            ]);
          })
          .then(function() {
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
        suiteSetup(function() {
          // Never show power settings.
          loadTimeData.overrideValues({
            enablePowerSettings: false,
          });
        });

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
        var idleSelect;
        var lidClosedToggle;

        suiteSetup(function() {
          // Always show power settings.
          loadTimeData.overrideValues({
            enablePowerSettings: true,
          });
        });

        setup(function() {
          return showAndGetDeviceSubpage('power', settings.routes.POWER)
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

                idleSelect = assert(powerPage.$$('#idleSelect'));
                lidClosedToggle = assert(powerPage.$$('#lidClosedToggle'));

                assertEquals(
                    1,
                    settings.DevicePageBrowserProxyImpl.getInstance()
                    .requestPowerManagementSettingsCalled_);
                sendPowerManagementSettings(
                    settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                    false /* idleControlled */,
                    settings.LidClosedBehavior.SUSPEND,
                    false /* lidClosedControlled */, true /* hasLid */);
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
          selectValue(powerSourceSelect, powerSourceSelect.children[1].value);
          expectEquals(
              powerSource.id,
              settings.DevicePageBrowserProxyImpl.getInstance().powerSourceId_);
        });

        test('set idle behavior', function() {
          selectValue(idleSelect, settings.IdleBehavior.DISPLAY_ON);
          expectEquals(
              settings.IdleBehavior.DISPLAY_ON,
              settings.DevicePageBrowserProxyImpl.getInstance().idleBehavior_);

          selectValue(idleSelect, settings.IdleBehavior.DISPLAY_OFF);
          expectEquals(
              settings.IdleBehavior.DISPLAY_OFF,
              settings.DevicePageBrowserProxyImpl.getInstance().idleBehavior_);
        });

        test('set lid behavior', function() {
          var sendLid = function(lidBehavior) {
            sendPowerManagementSettings(
                settings.IdleBehavior.DISPLAY_OFF,
                false /* idleControlled */, lidBehavior,
                false /* lidClosedControlled */, true /* hasLid */);
          };

          sendLid(settings.LidClosedBehavior.SUSPEND);
          assertTrue(lidClosedToggle.checked);

          MockInteractions.tap(lidClosedToggle.$$('#control'));
          expectEquals(
              settings.LidClosedBehavior.DO_NOTHING,
              settings.DevicePageBrowserProxyImpl.getInstance()
                  .lidClosedBehavior_);
          sendLid(settings.LidClosedBehavior.DO_NOTHING);
          expectFalse(lidClosedToggle.checked);

          MockInteractions.tap(lidClosedToggle.$$('#control'));
          expectEquals(
              settings.LidClosedBehavior.SUSPEND,
              settings.DevicePageBrowserProxyImpl.getInstance()
                  .lidClosedBehavior_);
          sendLid(settings.LidClosedBehavior.SUSPEND);
          expectTrue(lidClosedToggle.checked);
        });

        test('display idle and lid behavior', function() {
          return new Promise(function(resolve) {
            sendPowerManagementSettings(
                settings.IdleBehavior.DISPLAY_ON, false /* idleControlled */,
                settings.LidClosedBehavior.DO_NOTHING,
                false /* lidClosedControlled */, true /* hasLid */);
            powerPage.async(resolve);
          }).then(function() {
            expectEquals(
                settings.IdleBehavior.DISPLAY_ON.toString(), idleSelect.value);
            expectFalse(idleSelect.disabled);
            expectEquals(null, powerPage.$$('#idleControlledIndicator'));
            expectEquals(loadTimeData.getString('powerLidSleepLabel'),
                         lidClosedToggle.label);
            expectFalse(lidClosedToggle.checked);
            expectFalse(lidClosedToggle.isPrefEnforced());
          }).then(function() {
            sendPowerManagementSettings(
                settings.IdleBehavior.DISPLAY_OFF,
                false /* idleControlled */, settings.LidClosedBehavior.SUSPEND,
                false /* lidClosedControlled */, true /* hasLid */);
            return new Promise(function(resolve) { powerPage.async(resolve); });
          }).then(function() {
            expectEquals(settings.IdleBehavior.DISPLAY_OFF.toString(),
                         idleSelect.value);
            expectFalse(idleSelect.disabled);
            expectEquals(null, powerPage.$$('#idleControlledIndicator'));
            expectEquals(loadTimeData.getString('powerLidSleepLabel'),
                         lidClosedToggle.label);
            expectTrue(lidClosedToggle.checked);
            expectFalse(lidClosedToggle.isPrefEnforced());
          });
        });

        test('display controlled idle and lid behavior', function() {
          // When settings are controlled, the controls should be disabled and
          // the indicators should be shown.
          return new Promise(function(resolve) {
            sendPowerManagementSettings(
                settings.IdleBehavior.OTHER, true /* idleControlled */,
                settings.LidClosedBehavior.SHUT_DOWN,
                true /* lidClosedControlled */, true /* hasLid */);
            powerPage.async(resolve);
          }).then(function() {
            expectEquals(
                settings.IdleBehavior.OTHER.toString(), idleSelect.value);
            expectTrue(idleSelect.disabled);
            expectNotEquals(null, powerPage.$$('#idleControlledIndicator'));
            expectEquals(loadTimeData.getString('powerLidShutDownLabel'),
                         lidClosedToggle.label);
            expectTrue(lidClosedToggle.checked);
            expectTrue(lidClosedToggle.isPrefEnforced());
          }).then(function() {
            sendPowerManagementSettings(
                settings.IdleBehavior.DISPLAY_OFF,
                true /* idleControlled */,
                settings.LidClosedBehavior.STOP_SESSION,
                true /* lidClosedControlled */, true /* hasLid */);
            return new Promise(function(resolve) { powerPage.async(resolve); });
          }).then(function() {
            expectEquals(
                settings.IdleBehavior.DISPLAY_OFF.toString(),
                idleSelect.value);
            expectTrue(idleSelect.disabled);
            expectNotEquals(null, powerPage.$$('#idleControlledIndicator'));
            expectEquals(loadTimeData.getString('powerLidSignOutLabel'),
                         lidClosedToggle.label);
            expectTrue(lidClosedToggle.checked);
            expectTrue(lidClosedToggle.isPrefEnforced());
          });
        });

        test('hide lid behavior when lid not present', function() {
          return new Promise(function(resolve) {
            expectFalse(powerPage.$$('#lidClosedRow').hidden);
            sendPowerManagementSettings(
                settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                false /* idleControlled */, settings.LidClosedBehavior.SUSPEND,
                false /* lidClosedControlled */, false /* hasLid */);
            powerPage.async(resolve);
          }).then(function() {
            expectTrue(powerPage.$$('#lidClosedRow').hidden);
          });
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

      // Shorthand for settings.NoteAppLockScreenSupport.
      var LockScreenSupport;

      suiteSetup(function() {
        // Always show stylus settings.
        loadTimeData.overrideValues({
          stylusAllowed: true,
        });
      });

      setup(function() {
        return showAndGetDeviceSubpage('stylus', settings.routes.STYLUS)
            .then(function(page) {
              stylusPage = page;
              browserProxy = settings.DevicePageBrowserProxyImpl.getInstance();
              appSelector = assert(page.$$('#menu'));
              noAppsDiv = assert(page.$$('#no-apps'));
              waitingDiv = assert(page.$$('#waiting'));
              selectAppDiv = assert(page.$$('#select-app'));
              LockScreenSupport = settings.NoteAppLockScreenSupport;

              assertEquals(1, browserProxy.requestNoteTakingApps_);
              assert(browserProxy.onNoteTakingAppsUpdated_);
            });
      });

      // Helper function to allocate a note app entry.
      function entry(name, value, preferred, lockScreenSupport) {
        return {
          name: name,
          value: value,
          preferred: preferred,
          lockScreenSupport: lockScreenSupport
        };
      }

      /**  @return {?Element} */
      function noteTakingAppLockScreenSettings() {
        return stylusPage.$$('#note-taking-app-lock-screen-settings');
      }

      /** @return {?Element} */
      function enableAppOnLockScreenToggle() {
        return stylusPage.$$('#enable-app-on-lock-screen-toggle');
      }

      /** @return {?Element} */
      function enableAppOnLockScreenPolicyIndicator() {
        return stylusPage.$$("#enable-app-on-lock-screen-policy-indicator");
      }

      /** @return {?Element} */
      function enableAppOnLockScreenToggleLabel() {
        return stylusPage.$$('#lock-screen-toggle-label');
      }

      /** @return {?Element} */
      function keepLastNoteOnLockScreenToggle() {
        return stylusPage.$$('#keep-last-note-on-lock-screen-toggle');
      }

      /**
       * @param {?Element} element
       * @return {boolean}
       */
      function isVisible(element) {
        return !!element && element.offsetWidth > 0 && element.offsetHeight > 0;
      }

      test('choose first app if no preferred ones', function() {
        // Selector chooses the first value in list if there is no preferred
        // value set.
        browserProxy.setNoteTakingApps([
          entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
          entry('n2', 'v2', false, LockScreenSupport.NOT_SUPPORTED)
        ]);
        Polymer.dom.flush();
        assertEquals('v1', appSelector.value);
       });

       test('choose prefered app if exists', function() {
        // Selector chooses the preferred value if set.
        browserProxy.setNoteTakingApps([
          entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
          entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED)
        ]);
        Polymer.dom.flush();
        assertEquals('v2', appSelector.value);
      });

      test('change preferred app', function() {
        // Load app list.
        browserProxy.setNoteTakingApps([
          entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
          entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED)
        ]);
        Polymer.dom.flush();
        assertEquals(0, browserProxy.setPreferredAppCount_);
        assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());

        // Update select element to new value, verify browser proxy is called.
        appSelector.value = 'v1';
        stylusPage.onSelectedAppChanged_();
        assertEquals(1, browserProxy.setPreferredAppCount_);
        assertEquals('v1', browserProxy.getPreferredNoteTakingAppId());
      });

      test('preferred app does not change without interaction', function() {
        // Pass various types of data to page, verify the preferred note-app
        // does not change.
        browserProxy.setNoteTakingApps([]);
        Polymer.dom.flush();
        assertEquals('', browserProxy.getPreferredNoteTakingAppId());

        browserProxy.onNoteTakingAppsUpdated_([], true);
        Polymer.dom.flush();
        assertEquals('', browserProxy.getPreferredNoteTakingAppId());

        browserProxy.addNoteTakingApp(
            entry('n', 'v', false, LockScreenSupport.NOT_SUPPORTED));
        Polymer.dom.flush();
        assertEquals('', browserProxy.getPreferredNoteTakingAppId());

        browserProxy.setNoteTakingApps([
          entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
          entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED)
        ]);
        Polymer.dom.flush();
        assertEquals(0, browserProxy.setPreferredAppCount_);
        assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());
      });

      test('app-visibility', function() {
        // No apps available.
        browserProxy.setNoteTakingApps([]);
        assert(noAppsDiv.hidden);
        assert(!waitingDiv.hidden);
        assert(selectAppDiv.hidden);

        // Waiting for apps to finish loading.
        browserProxy.setAndroidAppsReceived(true);
        assert(!noAppsDiv.hidden);
        assert(waitingDiv.hidden);
        assert(selectAppDiv.hidden);

        // Apps loaded, show selector.
        browserProxy.addNoteTakingApp(
            entry('n', 'v', false, LockScreenSupport.NOT_SUPPORTED));
        assert(noAppsDiv.hidden);
        assert(waitingDiv.hidden);
        assert(!selectAppDiv.hidden);

        // Waiting for Android apps again.
        browserProxy.setAndroidAppsReceived(false);
        assert(noAppsDiv.hidden);
        assert(!waitingDiv.hidden);
        assert(selectAppDiv.hidden);

        browserProxy.setAndroidAppsReceived(true);
        assert(noAppsDiv.hidden);
        assert(waitingDiv.hidden);
        assert(!selectAppDiv.hidden);
      });

      test('enabled-on-lock-screen', function() {
        expectFalse(isVisible(noteTakingAppLockScreenSettings()));
        expectFalse(isVisible(enableAppOnLockScreenToggle()));
        expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

        return new Promise(function(resolve) {
          // No apps available.
          browserProxy.setNoteTakingApps([]);
          stylusPage.async(resolve);
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(noteTakingAppLockScreenSettings()));
          expectFalse(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

          // Single app which does not support lock screen note taking.
          browserProxy.addNoteTakingApp(
              entry('n1', 'v1', true, LockScreenSupport.NOT_SUPPORTED));
          return new Promise(function(resolve) {stylusPage.async(resolve);});
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(noteTakingAppLockScreenSettings()));
          expectFalse(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

          // Add an app with lock screen support, but do not select it yet.
          browserProxy.addNoteTakingApp(
              entry('n2', 'v2', false, LockScreenSupport.SUPPORTED));
          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(noteTakingAppLockScreenSettings()));
          expectFalse(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

          // Select the app with lock screen app support.
          appSelector.value = 'v2';
          stylusPage.onSelectedAppChanged_();
          assertEquals(1, browserProxy.setPreferredAppCount_);
          assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());

          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectTrue(isVisible(noteTakingAppLockScreenSettings()));
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);

          // Preferred app updated to be enabled on lock screen.
          browserProxy.setNoteTakingApps([
            entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
            entry('n2', 'v2', true, LockScreenSupport.ENABLED)
          ]);
          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectTrue(isVisible(noteTakingAppLockScreenSettings()));
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectTrue(enableAppOnLockScreenToggle().checked);

          // Select the app that does not support lock screen again.
          appSelector.value = 'v1';
          stylusPage.onSelectedAppChanged_();
          assertEquals(2, browserProxy.setPreferredAppCount_);
          assertEquals('v1', browserProxy.getPreferredNoteTakingAppId());

          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(noteTakingAppLockScreenSettings()));
          expectFalse(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
        });
      });

      test('initial-app-lock-screen-enabled', function() {
        return new Promise(function(resolve) {
          browserProxy.setNoteTakingApps([
            entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)
          ]);
          stylusPage.async(resolve);
        }).then(function() {
          Polymer.dom.flush();

          expectTrue(isVisible(noteTakingAppLockScreenSettings()));
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

          browserProxy.setNoteTakingApps([
            entry('n1', 'v1', true, LockScreenSupport.ENABLED)
          ]);

          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectTrue(isVisible(noteTakingAppLockScreenSettings()));
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectTrue(enableAppOnLockScreenToggle().checked);
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

          browserProxy.setNoteTakingApps([
            entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)
          ]);

          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectTrue(isVisible(noteTakingAppLockScreenSettings()));
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

          browserProxy.setNoteTakingApps([]);
          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(enableAppOnLockScreenToggle()));
        });
      });

      test('tap-on-enable-note-taking-on-lock-screen', function() {
        return new Promise(function(resolve) {
          browserProxy.setNoteTakingApps([
            entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)
          ]);
          stylusPage.async(resolve);
        }).then(function() {
          Polymer.dom.flush();

          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

          MockInteractions.tap(enableAppOnLockScreenToggle());
          assertEquals(1, browserProxy.setAppOnLockScreenCount_);

          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectTrue(enableAppOnLockScreenToggle().checked);
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

          expectEquals(LockScreenSupport.ENABLED,
                       browserProxy.getPreferredAppLockScreenState());

          MockInteractions.tap(enableAppOnLockScreenToggle());
          assertEquals(2, browserProxy.setAppOnLockScreenCount_);

          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
          expectFalse(enableAppOnLockScreenToggle().checked);
          expectEquals(LockScreenSupport.SUPPORTED,
                       browserProxy.getPreferredAppLockScreenState());
        });
      });

      test('tap-on-enable-note-taking-on-lock-screen-label', function() {
        return new Promise(function(resolve) {
          browserProxy.setNoteTakingApps([
            entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)
          ]);
          stylusPage.async(resolve);
        }).then(function() {
          Polymer.dom.flush();

          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);

          MockInteractions.tap(enableAppOnLockScreenToggleLabel());
          assertEquals(1, browserProxy.setAppOnLockScreenCount_);

          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectTrue(enableAppOnLockScreenToggle().checked);
          expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

          expectEquals(LockScreenSupport.ENABLED,
                       browserProxy.getPreferredAppLockScreenState());

          MockInteractions.tap(enableAppOnLockScreenToggleLabel());
          assertEquals(2, browserProxy.setAppOnLockScreenCount_);

          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(enableAppOnLockScreenToggle().checked);
          expectEquals(LockScreenSupport.SUPPORTED,
                       browserProxy.getPreferredAppLockScreenState());
        });
      });

      test('lock-screen-apps-disabled-by-policy', function() {
        expectFalse(isVisible(enableAppOnLockScreenToggle()));
        expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

        return new Promise(function(resolve) {
          // Add an app with lock screen support.
          browserProxy.addNoteTakingApp(
              entry('n2', 'v2', true, LockScreenSupport.NOT_ALLOWED_BY_POLICY));
          stylusPage.async(resolve);
        }).then(function() {
          Polymer.dom.flush();
          expectTrue(isVisible(noteTakingAppLockScreenSettings()));
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);
          expectTrue(isVisible(enableAppOnLockScreenPolicyIndicator()));

          // The toggle should be disabled, so enabling app on lock screen
          // should not be attempted.
          MockInteractions.tap(enableAppOnLockScreenToggle());
          assertEquals(0, browserProxy.setAppOnLockScreenCount_);

          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();

          // Tap on label should not work either.
          MockInteractions.tap(enableAppOnLockScreenToggleLabel());
          assertEquals(0, browserProxy.setAppOnLockScreenCount_);

          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectTrue(isVisible(noteTakingAppLockScreenSettings()));
          assert(isVisible(enableAppOnLockScreenToggle()));
          expectFalse(enableAppOnLockScreenToggle().checked);
          expectTrue(isVisible(enableAppOnLockScreenPolicyIndicator()));

          expectEquals(LockScreenSupport.NOT_ALLOWED_BY_POLICY,
                       browserProxy.getPreferredAppLockScreenState());
        });
      });

      test('keep-last-note-on-lock-screen', function() {
        return new Promise(function(resolve) {
          browserProxy.setNoteTakingApps([
            entry('n1', 'v1', true, LockScreenSupport.NOT_SUPPORTED),
            entry('n2', 'v2', false, LockScreenSupport.SUPPORTED)
          ]);
          stylusPage.async(resolve);
        }).then(function() {
          Polymer.dom.flush();
          expectFalse(isVisible(noteTakingAppLockScreenSettings()));
          expectFalse(isVisible(keepLastNoteOnLockScreenToggle()));

          browserProxy.setNoteTakingApps([
            entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
            entry('n2', 'v2', true, LockScreenSupport.SUPPORTED)
          ]);
          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectTrue(isVisible(noteTakingAppLockScreenSettings()));
          expectFalse(isVisible(keepLastNoteOnLockScreenToggle()));

          browserProxy.setNoteTakingApps([
            entry('n2', 'v2', true, LockScreenSupport.ENABLED),
          ]);
          return new Promise(function(resolve) { stylusPage.async(resolve); });
        }).then(function() {
          Polymer.dom.flush();
          expectTrue(isVisible(noteTakingAppLockScreenSettings()));
          assert(isVisible(keepLastNoteOnLockScreenToggle()));
          expectTrue(keepLastNoteOnLockScreenToggle().checked);

          // Clicking the toggle updates the pref value.
          MockInteractions.tap(keepLastNoteOnLockScreenToggle().$$('#control'));
          expectFalse(keepLastNoteOnLockScreenToggle().checked);

          expectFalse(
              devicePage.prefs.settings.restore_last_lock_screen_note.value);

          // Changing the pref value updates the toggle.
          devicePage.set(
              'prefs.settings.restore_last_lock_screen_note.value', true);
          expectTrue(keepLastNoteOnLockScreenToggle().checked);
        });
      });
    });
  });

  return {
    TestNames: TestNames
  };
});
