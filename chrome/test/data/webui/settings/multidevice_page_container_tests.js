// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Multidevice', function() {
  let multidevicePageContainer = null;
  let ALL_MODES;
  let HOST_SET_MODES;

  suiteSetup(function() {
    ALL_MODES = Object.values(settings.MultiDeviceSettingsMode);
    HOST_SET_MODES = [
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ];
  });

  setup(function() {
    PolymerTest.clearBody();
    multidevicePageContainer =
        document.createElement('settings-multidevice-page-container');
    assertTrue(!!multidevicePageContainer);

    document.body.appendChild(multidevicePageContainer);
    Polymer.dom.flush();
  });

  teardown(function() {
    multidevicePageContainer.remove();
  });

  const getFakePageContentData = function(mode) {
    return {
      mode: mode,
      hostDevice: HOST_SET_MODES.includes(mode) ? {name: 'Pixel XL'} : null,
    };
  };

  const setMode = function(mode) {
    multidevicePageContainer.onStatusChanged_(getFakePageContentData(mode));
    Polymer.dom.flush();
  };

  const getMultidevicePage = () =>
      multidevicePageContainer.$$('settings-multidevice-page');

  test('mode toggles multidevice page', function() {
    // Check that the settings-page is visible iff the mode is not
    // NO_ELIGIBLE_HOSTS.
    for (let mode of ALL_MODES) {
      setMode(mode);
      assertEquals(
          !getMultidevicePage(),
          mode === settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
    }
    // One more loop to ensure we transition in and out of NO_ELIGIBLE_HOSTS
    // mode.
    for (let mode of ALL_MODES) {
      setMode(mode);
      assertEquals(
          !getMultidevicePage(),
          mode === settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
    }
  });

  test(
      'pageContentData_ property passes to multidevice page if present',
      function() {
        for (let mode of ALL_MODES) {
          setMode(mode);
          if (mode === settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS)
            assertEquals(getMultidevicePage(), null);
          else
            assertDeepEquals(
                multidevicePageContainer.pageContentData_,
                getMultidevicePage().pageContentData);
        }
      });
});
