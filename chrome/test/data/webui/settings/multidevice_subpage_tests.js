// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Multidevice', function() {
  let multideviceSubpage = null;
  // Although HOST_SET_MODES is effectively a constant, it cannot reference the
  // enum settings.MultiDeviceSettingsMode from here so its initialization is
  // deferred to the suiteSetup function.
  let HOST_SET_MODES;
  const HOST_DEVICE = {
    name: 'Pixel XL',
  };

  function setPageContentData(newMode) {
    multideviceSubpage.pageContentData = {
      mode: newMode,
      hostDevice: HOST_DEVICE,
    };
    Polymer.dom.flush();
  }

  suiteSetup(function() {
    HOST_SET_MODES = [
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ];
  });

  setup(function() {
    multideviceSubpage = document.createElement('settings-multidevice-subpage');

    setPageContentData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    multideviceSubpage.prefs = {
      multidevice: {sms_connect_enabled: {value: true}},
      multidevice_setup: {suite_enabled: {value: true}},
    };

    document.body.appendChild(multideviceSubpage);
    Polymer.dom.flush();
  });

  teardown(function() {
    multideviceSubpage.remove();
  });

  test('individual features appear only if host is verified', function() {
    for (const mode of HOST_SET_MODES) {
      setPageContentData(mode);
      assertEquals(
          !!multideviceSubpage.$$('settings-multidevice-feature-item'),
          mode == settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    }
  });

  test('clicking EasyUnlock item routes to screen lock page', function() {
    multideviceSubpage.$$('#smart-lock-item').$.card.click();
    assertEquals(settings.getCurrentRoute(), settings.routes.LOCK_SCREEN);
  });

  test('AndroidMessages item shows correct input control', function() {
    const inputControl = multideviceSubpage.$$('[slot=feature-controller]');

    multideviceSubpage.androidMessagesRequiresSetup_ = true;
    Polymer.dom.flush();
    assertTrue(!!inputControl.querySelector('paper-button'));
    assertFalse(
        !!inputControl.querySelector('settings-multidevice-feature-toggle'));

    multideviceSubpage.androidMessagesRequiresSetup_ = false;
    Polymer.dom.flush();
    assertFalse(!!inputControl.querySelector('paper-button'));
    assertTrue(
        !!inputControl.querySelector('settings-multidevice-feature-toggle'));
  });
});
