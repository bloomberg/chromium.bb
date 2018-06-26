// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Multidevice', function() {
  let multidevicePageContainer = null;

  let ALL_MODES;

  suiteSetup(function() {});

  setup(function() {
    PolymerTest.clearBody();
    multidevicePageContainer =
        document.createElement('settings-multidevice-page-container');
    assertTrue(!!multidevicePageContainer);

    ALL_MODES = Object.values(settings.MultiDeviceSettingsMode);

    document.body.appendChild(multidevicePageContainer);
    Polymer.dom.flush();
  });

  teardown(function() {
    multidevicePageContainer.remove();
  });

  const setMode = function(mode) {
    multidevicePageContainer.mode_ = mode;
    Polymer.dom.flush();
  };

  const getMultidevicePage = () =>
      multidevicePageContainer.$$('settings-multidevice-page');

  test('mode_ property toggles multidevice page', function() {
    // Check that the settings-page is visible iff the mode is not
    // NO_ELIGIBLE_HOSTS.
    for (let mode of ALL_MODES) {
      setMode(mode);
      assertEquals(
          !getMultidevicePage(),
          mode == settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
    }
    // One more loop to ensure we transition in and out of NO_ELIGIBLE_HOSTS
    // mode.
    for (let mode of ALL_MODES) {
      setMode(mode);
      assertEquals(
          !getMultidevicePage(),
          mode == settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
    }
  });

  test('mode_ property passes to multidevice page if present', function() {
    for (let mode of ALL_MODES) {
      setMode(mode);
      if (mode == settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS)
        assertEquals(getMultidevicePage(), null);
      else
        assertEquals(multidevicePageContainer.mode_, getMultidevicePage().mode);
    }
  });
});
