// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  function getFakePrefs() {
    return {
      cros: {
        system: {
          timezone: {
            key: 'cros.system.timezone',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: 'Westeros/Kings_Landing',
          },
        },
        flags: {
          // TODO(alemate): This test should be run for all possible
          // combinations of values of these options.
          per_user_timezone_enabled: {
            key: 'cros.flags.per_user_timezone_enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
          fine_grained_time_zone_detection_enabled: {
            key: 'cros.flags.fine_grained_time_zone_detection_enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
      },
      settings: {
        clock: {
          use_24hour_clock: {
            key: 'settings.clock.use_24hour_clock',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
        resolve_timezone_by_geolocation_method: {
          key: 'settings.resolve_timezone_by_geolocation_method',
          type: settings.TimeZoneAutoDetectMethod,
          value: settings.TimeZoneAutoDetectMethod.IP_ONLY,
        },
        resolve_device_timezone_by_geolocation_policy: {
          key: 'settings.resolve_device_timezone_by_geolocation_policy',
          type: settings.SystemTimezoneProto,
          value: settings.SystemTimezoneProto.USERS_DECIDE,
        },
        timezone: {
          key: 'settings.timezone',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: 'Westeros/Kings_Landing',
        },
      },
    };
  }

  function updatePrefsWithPolicy(prefs, managed, valueFromPolicy) {
    var prefsCopy = JSON.parse(JSON.stringify(prefs));
    if (managed) {
      prefsCopy.settings.resolve_timezone_by_geolocation_method.controlledBy =
          chrome.settingsPrivate.ControlledBy.USER_POLICY;
      prefsCopy.settings.resolve_timezone_by_geolocation_method.enforcement =
          chrome.settingsPrivate.Enforcement.ENFORCED;
      prefsCopy.settings.resolve_timezone_by_geolocation_method.value =
          valueFromPolicy ? settings.TimeZoneAutoDetectMethod.IP_ONLY :
                            settings.TimeZoneAutoDetectMethod.DISABLED;
      prefsCopy.settings.resolve_timezone_by_geolocation_method.controlledBy =
          chrome.settingsPrivate.ControlledBy.USER_POLICY;
      prefsCopy.settings.resolve_timezone_by_geolocation_method.enforcement =
          chrome.settingsPrivate.Enforcement.ENFORCED;
      prefsCopy.settings.resolve_device_timezone_by_geolocation_policy.value =
          valueFromPolicy ? settings.SystemTimezoneProto.IP_ONLY :
                            settings.SystemTimezoneProto.DISABLED;
      prefsCopy.settings.resolve_device_timezone_by_geolocation_policy
          .controlledBy = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
      prefsCopy.settings.resolve_device_timezone_by_geolocation_policy
          .enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      prefsCopy.settings.timezone.controlledBy =
          chrome.settingsPrivate.ControlledBy.USER_POLICY;
      prefsCopy.settings.timezone.enforcement =
          chrome.settingsPrivate.Enforcement.ENFORCED;
    } else {
      prefsCopy.settings.resolve_timezone_by_geolocation_method.controlledBy =
          undefined;
      prefsCopy.settings.resolve_timezone_by_geolocation_method.enforcement =
          undefined;
      // Auto-resolve defaults to true.
      prefsCopy.settings.resolve_timezone_by_geolocation_method.value =
          settings.TimeZoneAutoDetectMethod.IP_ONLY;
      prefsCopy.settings.timezone.controlledBy = undefined;
      prefsCopy.settings.timezone.enforcement = undefined;
    }
    return prefsCopy;
  }

  /**
   * Sets up fakes and creates the date time element.
   * @param {!Object} prefs
   * @param {boolean} hasPolicy
   * @param {boolean=} opt_autoDetectPolicyValue
   * @return {!SettingsDateTimePage}
   */
  function initializeDateTime(prefs, hasPolicy, opt_autoDetectPolicyValue) {
    // Find the desired initial time zone by ID.
    var timeZone = assert(fakeTimeZones.find(function(timeZonePair) {
      return timeZonePair[0] == prefs.cros.system.timezone.value;
    }));

    var data = {
      timeZoneID: timeZone[0],
      timeZoneName: timeZone[1],
      controlledSettingPolicy: 'This setting is enforced by your administrator',
      setTimeZoneAutomaticallyDisabled:
          'Automatic time zone detection disabled.',
      setTimeZoneAutomaticallyIpOnlyDefault:
          'Automatic time zone detection IP-only.',
      setTimeZoneAutomaticallyIpOnlyDefaultDescription:
          'Automatic time zone detection IP-only description.',
      setTimeZoneAutomaticallyWithWiFiAccessPointsData:
          'Automatic time zone detection with WiFi AP',
      setTimeZoneAutomaticallyWithWiFiAccessPointsDataDescription:
          'Automatic time zone detection with WiFi AP description',
      setTimeZoneAutomaticallyWithAllLocationInfo:
          'Automatic time zone detection with all location info',
      setTimeZoneAutomaticallyWithAllLocationInfoDescription:
          'Automatic time zone detection with all location info description',
    };

    if (hasPolicy)
      data.timeZoneAutoDetectValueFromPolicy = opt_autoDetectPolicyValue;

    window.loadTimeData = new LoadTimeData;
    loadTimeData.data = data;

    var dateTime = document.createElement('settings-date-time-page');
    dateTime.prefs =
        updatePrefsWithPolicy(prefs, hasPolicy, opt_autoDetectPolicyValue);
    CrSettingsPrefs.setInitialized();

    document.body.appendChild(dateTime);
    cr.webUIListenerCallback(
        'time-zone-auto-detect-policy', hasPolicy, opt_autoDetectPolicyValue);

    return dateTime;
  }

  // CrOS sends time zones as [id, friendly name] pairs.
  var fakeTimeZones = [
    ['Westeros/Highgarden', '(KNG-2:00) The Reach Time (Highgarden)'],
    ['Westeros/Winterfell', '(KNG-1:00) The North Time (Winterfell)'],
    ['Westeros/Kings_Landing',
     '(KNG+0:00) Westeros Standard Time (King\'s Landing)'],
    ['Westeros/TheEyrie', '(KNG+1:00) The Vale Time (The Eyrie)'],
    ['Westeros/Sunspear', '(KNG+2:00) Dorne Time (Sunspear)'],
    ['FreeCities/Pentos', '(KNG+6:00) Pentos Time (Pentos)'],
    ['FreeCities/Volantis', '(KNG+9:00) Volantis Time (Volantis)'],
    ['BayOfDragons/Daenerys', '(KNG+14:00) Daenerys Free Time (Meereen)'],
  ];

  suite('settings-date-time-page', function() {
    var dateTime;

    // Track whether handler functions have been called.
    var dateTimePageReadyCalled;
    var getTimeZonesCalled;

    setup(function() {
      PolymerTest.clearBody();
      CrSettingsPrefs.resetForTesting();

      dateTimePageReadyCalled = false;
      getTimeZonesCalled = false;

      registerMessageCallback('dateTimePageReady', null, function() {
        assertFalse(dateTimePageReadyCalled);
        dateTimePageReadyCalled = true;
      });

      registerMessageCallback('getTimeZones', null, function(callback) {
        assertFalse(getTimeZonesCalled);
        getTimeZonesCalled = true;
        cr.webUIResponse(callback, true, fakeTimeZones);
      });
    });

    suiteTeardown(function() {
      // TODO(michaelpg): Removes the element before exiting, because the
      // <paper-tooltip> in <cr-policy-indicator> somehow causes warnings
      // and/or script errors in axs_testing.js.
      PolymerTest.clearBody();
    });

    function popuateSubpage() {
      var timeZoneSettingsSubmenuButton =
          dateTime.$$('#timeZoneSettingsTrigger');
      MockInteractions.tap(timeZoneSettingsSubmenuButton);
      Polymer.dom.flush();
    }

    function getTimeZoneSelector(id) {
      return dateTime.$$('timezone-selector').$$(id);
    }

    function verifyAutoDetectSetting(autoDetect, managed) {
      var selector = getTimeZoneSelector('#userTimeZoneSelector');
      var selectorHidden = selector ? selector.hidden : true;
      assertEquals(managed || autoDetect, selectorHidden);

      var checkButton = dateTime.$$('#timeZoneAutoDetect');
      var checkButtonChecked = checkButton ? checkButton.checked : false;
      if (!managed)
        assertEquals(autoDetect, checkButtonChecked);
    }

    function verifyPolicy(policy) {
      var indicator = dateTime.$$('cr-policy-indicator');
      if (indicator && indicator.style.display == 'none')
        indicator = null;

      if (policy) {
        assertTrue(!!indicator);
        assertTrue(indicator.indicatorVisible);
      } else {
        // Indicator should be missing dom-ifed out.
        assertFalse(!!indicator);
      }

      assertEquals(
          policy, dateTime.$$('#timeZoneAutoDetect').disabled);
    }

    function verifyTimeZonesPopulated(populated) {
      var userTimezoneDropdown = getTimeZoneSelector('#userTimeZoneSelector');
      var systemTimezoneDropdown =
          getTimeZoneSelector('#systemTimezoneSelector');

      var dropdown =
          userTimezoneDropdown ? userTimezoneDropdown : systemTimezoneDropdown;
      if (populated)
        assertEquals(fakeTimeZones.length, dropdown.menuOptions.length);
      else
        assertEquals(1, dropdown.menuOptions.length);
    }

    function updatePolicy(dateTime, managed, valueFromPolicy) {
      dateTime.prefs =
          updatePrefsWithPolicy(dateTime.prefs, managed, valueFromPolicy);
      cr.webUIListenerCallback(
          'time-zone-auto-detect-policy', managed, valueFromPolicy);
    }

    test('auto-detect on', function(done) {
      var prefs = getFakePrefs();
      dateTime = initializeDateTime(prefs, false);

      assertTrue(dateTimePageReadyCalled);
      assertFalse(getTimeZonesCalled);

      Polymer.dom.flush();
      verifyAutoDetectSetting(true, false);
      verifyTimeZonesPopulated(false);
      verifyPolicy(false);

      // Disable auto-detect.
      MockInteractions.tap(dateTime.$$('#timeZoneAutoDetect'));
      Polymer.dom.flush();
      verifyAutoDetectSetting(false, false);
      assertTrue(getTimeZonesCalled);

      setTimeout(function() {
        verifyTimeZonesPopulated(true);
        done();
      });
    });

    test('auto-detect off', function(done) {
      dateTime = initializeDateTime(getFakePrefs(), false);
      setTimeout(function() {
        dateTime.set(
            'prefs.settings.resolve_timezone_by_geolocation_method.value',
            settings.TimeZoneAutoDetectMethod.DISABLED);
      });

      setTimeout(function() {
        assertTrue(dateTimePageReadyCalled);
        assertTrue(getTimeZonesCalled);

        verifyAutoDetectSetting(false, false);
        verifyPolicy(false);

        verifyTimeZonesPopulated(true);

        // Enable auto-detect.
        MockInteractions.tap(dateTime.$$('#timeZoneAutoDetect'));
      });

      setTimeout(function() {
        verifyAutoDetectSetting(true);
        done();
      });
    });

    test('auto-detect forced on', function(done) {
      var prefs = getFakePrefs();
      dateTime = initializeDateTime(prefs, true, true);
      setTimeout(function() {
        dateTime.set(
            'prefs.settings.resolve_timezone_by_geolocation_method.value',
            settings.TimeZoneAutoDetectMethod.DISABLED);
      });

      setTimeout(function() {
        assertTrue(dateTimePageReadyCalled);
        assertFalse(getTimeZonesCalled);

        verifyAutoDetectSetting(true, true);
        verifyTimeZonesPopulated(false);
        verifyPolicy(true);

        // Cannot disable auto-detect.
        MockInteractions.tap(dateTime.$$('#timeZoneAutoDetect'));
      });

      setTimeout(function() {
        verifyAutoDetectSetting(true, true);
        assertFalse(getTimeZonesCalled);

        // Update the policy: force auto-detect off.
        updatePolicy(dateTime, true, false);
      });
      setTimeout(function() {
        verifyAutoDetectSetting(false, true);
        verifyPolicy(true);

        assertTrue(getTimeZonesCalled);
        verifyTimeZonesPopulated(true);
        done();
      });
    });

    test('auto-detect forced off', function(done) {
      var prefs = getFakePrefs();
      dateTime = initializeDateTime(prefs, true, false);

      setTimeout(function() {
        assertTrue(dateTimePageReadyCalled);
        assertTrue(getTimeZonesCalled);

        verifyAutoDetectSetting(false, true);
        verifyPolicy(true);
        verifyTimeZonesPopulated(true);

        // Remove the policy so user's preference takes effect.
        updatePolicy(dateTime, false, false);
      });
      setTimeout(function() {
        verifyAutoDetectSetting(true, false);
        verifyPolicy(false);

        // User can disable auto-detect.
        MockInteractions.tap(dateTime.$$('#timeZoneAutoDetect'));
      });
      setTimeout(function() {
        verifyAutoDetectSetting(false, false);
        done();
      });
    });

    test('set date and time button', function() {
      dateTime = initializeDateTime(getFakePrefs(), false);

      var showSetDateTimeUICalled = false;
      registerMessageCallback('showSetDateTimeUI', null, function() {
        assertFalse(showSetDateTimeUICalled);
        showSetDateTimeUICalled = true;
      });

      setTimeout(function() {
        var setDateTimeButton = dateTime.$$('#setDateTime');
        assertEquals(0, setDateTimeButton.offsetHeight);

        // Make the date and time editable.
        cr.webUIListenerCallback('can-set-date-time-changed', true);
        assertGT(setDateTimeButton.offsetHeight, 0);

        assertFalse(showSetDateTimeUICalled);
        MockInteractions.tap(setDateTimeButton);
      });
      setTimeout(function() {
        assertTrue(showSetDateTimeUICalled);

        // Make the date and time not editable.
        cr.webUIListenerCallback('can-set-date-time-changed', false);
        assertEquals(setDateTimeButton.offsetHeight, 0);
      });
    });
  });
})();
