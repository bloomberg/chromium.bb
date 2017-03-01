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
      },
      settings: {
        clock: {
          use_24hour_clock: {
            key: 'settings.clock.use_24hour_clock',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
        resolve_timezone_by_geolocation: {
          key: 'settings.resolve_timezone_by_geolocation',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
    };
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
    };

    if (hasPolicy)
      data.timeZoneAutoDetectValueFromPolicy = opt_autoDetectPolicyValue;

    window.loadTimeData = new LoadTimeData;
    loadTimeData.data = data;

    var dateTime = document.createElement('settings-date-time-page');
    dateTime.prefs = prefs;
    CrSettingsPrefs.setInitialized();

    document.body.appendChild(dateTime);
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
      // <paper-tooltip> in <cr-policy-pref-indicator> somehow causes warnings
      // and/or script errors in axs_testing.js.
      PolymerTest.clearBody();
    });

    function verifyAutoDetectSetting(autoDetect) {
      assertEquals(autoDetect, dateTime.$$('settings-dropdown-menu').disabled);
      assertEquals(autoDetect, dateTime.$.timeZoneAutoDetect.checked);
    }

    function verifyPolicy(policy) {
      Polymer.dom.flush();
      var indicator = dateTime.$$('cr-policy-pref-indicator');

      if (policy) {
        assertTrue(!!indicator);
        assertTrue(indicator.indicatorVisible);
      } else {
        // Indicator should be missing dom-ifed out.
        assertFalse(!!indicator);
      }

      assertEquals(policy, dateTime.$.timeZoneAutoDetect.disabled);
    }

    function verifyTimeZonesPopulated(populated) {
      var dropdown = dateTime.$$('settings-dropdown-menu');
      if (populated)
        assertEquals(fakeTimeZones.length, dropdown.menuOptions.length);
      else
        assertEquals(1, dropdown.menuOptions.length);
    }

    test('auto-detect on', function(done) {
      var prefs = getFakePrefs();
      dateTime = initializeDateTime(prefs, false);

      assertTrue(dateTimePageReadyCalled);
      assertFalse(getTimeZonesCalled);

      verifyAutoDetectSetting(true);
      verifyTimeZonesPopulated(false);
      verifyPolicy(false);

      // Disable auto-detect.
      MockInteractions.tap(dateTime.$.timeZoneAutoDetect);
      verifyAutoDetectSetting(false);
      assertTrue(getTimeZonesCalled);

      setTimeout(function() {
        verifyTimeZonesPopulated(true);
        done();
      });
    });

    test('auto-detect off', function(done) {
      var prefs = getFakePrefs();
      prefs.settings.resolve_timezone_by_geolocation.value = false;
      dateTime = initializeDateTime(prefs, false);
      cr.webUIListenerCallback('time-zone-auto-detect-policy', false);

      assertTrue(dateTimePageReadyCalled);
      assertTrue(getTimeZonesCalled);

      verifyAutoDetectSetting(false);
      verifyPolicy(false);

      setTimeout(function() {
        verifyTimeZonesPopulated(true);

        // Enable auto-detect.
        MockInteractions.tap(dateTime.$.timeZoneAutoDetect);
        verifyAutoDetectSetting(true);
        done();
      });
    });

    test('auto-detect forced on', function(done) {
      var prefs = getFakePrefs();
      prefs.settings.resolve_timezone_by_geolocation.value = false;
      dateTime = initializeDateTime(prefs, true, true);
      cr.webUIListenerCallback('time-zone-auto-detect-policy', true, true);

      assertTrue(dateTimePageReadyCalled);
      assertFalse(getTimeZonesCalled);

      verifyAutoDetectSetting(true);
      verifyTimeZonesPopulated(false);
      verifyPolicy(true);

      // Cannot disable auto-detect.
      MockInteractions.tap(dateTime.$.timeZoneAutoDetect);
      verifyAutoDetectSetting(true);
      assertFalse(getTimeZonesCalled);

      // Update the policy: force auto-detect off.
      cr.webUIListenerCallback('time-zone-auto-detect-policy', true, false);
      verifyAutoDetectSetting(false);
      verifyPolicy(true);

      assertTrue(getTimeZonesCalled);
      setTimeout(function() {
        verifyTimeZonesPopulated(true);
        done();
      });
    });

    test('auto-detect forced off', function(done) {
      var prefs = getFakePrefs();
      dateTime = initializeDateTime(prefs, true, false);
      cr.webUIListenerCallback('time-zone-auto-detect-policy', true, false);

      assertTrue(dateTimePageReadyCalled);
      assertTrue(getTimeZonesCalled);

      verifyAutoDetectSetting(false);
      verifyPolicy(true);

      setTimeout(function() {
        verifyTimeZonesPopulated(true);

        // Remove the policy so user's preference takes effect.
        cr.webUIListenerCallback('time-zone-auto-detect-policy', false);
        verifyAutoDetectSetting(true);
        verifyPolicy(false);

        // User can disable auto-detect.
        MockInteractions.tap(dateTime.$.timeZoneAutoDetect);
        verifyAutoDetectSetting(false);
        done();
      });
    });

    test('set date and time button', function() {
      dateTime = initializeDateTime(getFakePrefs(), false);
      cr.webUIListenerCallback('time-zone-auto-detect-policy', false);

      var showSetDateTimeUICalled = false;
      registerMessageCallback('showSetDateTimeUI', null, function() {
        assertFalse(showSetDateTimeUICalled);
        showSetDateTimeUICalled = true;
      });

      var setDateTimeButton = dateTime.$.setDateTime;
      assertEquals(0, setDateTimeButton.offsetHeight);

      // Make the date and time editable.
      cr.webUIListenerCallback('can-set-date-time-changed', true);
      assertGT(setDateTimeButton.offsetHeight, 0);

      assertFalse(showSetDateTimeUICalled);
      MockInteractions.tap(setDateTimeButton);
      assertTrue(showSetDateTimeUICalled);

      // Make the date and time not editable.
      cr.webUIListenerCallback('can-set-date-time-changed', false);
      assertEquals(setDateTimeButton.offsetHeight, 0);
    });
  });
})();
