// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {settings.MultideviceBrowserProxy}
 * Note: Only showMultiDeviceSetupDialog is used by the multidevice-page
 * element.
 */
class TestMultideviceBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'showMultiDeviceSetupDialog',
      'getPageContentData',
    ]);
  }

  /** @override */
  showMultiDeviceSetupDialog() {
    this.methodCalled('showMultiDeviceSetupDialog');
  }
}

suite('Multidevice', function() {
  let multidevicePage = null;
  let browserProxy = null;
  let HOST_SET_MODES;
  const HOST_DEVICE = {
    name: 'Pixel XL',
  };

  suiteSetup(function() {
    HOST_SET_MODES = [
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ];
  });

  setup(function() {
    browserProxy = new TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    multidevicePage = document.createElement('settings-multidevice-page');
    assertTrue(!!multidevicePage);

    document.body.appendChild(multidevicePage);
    Polymer.dom.flush();
  });

  teardown(function() {
    multidevicePage.remove();
  });

  const setPageContentData = function(newMode, newHostDevice) {
    multidevicePage.pageContentData = {
      mode: newMode,
      hostDevice: newHostDevice,
    };
    Polymer.dom.flush();
  };

  const getLabel = () => multidevicePage.$$('#multidevice-label').textContent;

  test('pressing setup shows multidevice setup dialog', function() {
    setPageContentData(settings.MultiDeviceSettingsMode.NO_HOST_SET, null);
    const button = multidevicePage.$$('paper-button');
    assertTrue(!!button);
    button.click();
    return browserProxy.whenCalled('showMultiDeviceSetupDialog');
  });

  test('headings render based on mode and host', function() {
    for (let mode of HOST_SET_MODES) {
      setPageContentData(mode, HOST_DEVICE);
      assertEquals(getLabel(), HOST_DEVICE.name);
    }
    setPageContentData(settings.MultiDeviceSettingsMode.NO_HOST_SET, null);
    assertNotEquals(getLabel(), HOST_DEVICE.name);
  });

  test('changing host device and fixing mode changes header', function() {
    setPageContentData(
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED, HOST_DEVICE);
    assertEquals(getLabel(), HOST_DEVICE.name);
    const anotherHost =
        Object.assign(HOST_DEVICE, {name: 'Super Duper ' + HOST_DEVICE.name});
    setPageContentData(
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED, anotherHost);
    assertEquals(getLabel(), anotherHost.name);
  });
});
