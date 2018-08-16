// Copyright 2018 The Chromium Authors. All rights reserved.
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
  const HOST_DEVICE = 'Pixel XL';

  function setPageContentData(newMode, newHostDeviceName) {
    multidevicePage.pageContentData = {
      mode: newMode,
      hostDeviceName: newHostDeviceName,
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

  const getLabel = () => multidevicePage.$$('#multidevice-label').textContent;

  const getSubpage = () => multidevicePage.$$('settings-multidevice-subpage');

  test('clicking setup shows multidevice setup dialog', function() {
    setPageContentData(settings.MultiDeviceSettingsMode.NO_HOST_SET, undefined);
    const button = multidevicePage.$$('paper-button');
    assertTrue(!!button);
    button.click();
    return browserProxy.whenCalled('showMultiDeviceSetupDialog');
  });

  test('headings render based on mode and host', function() {
    for (let mode of HOST_SET_MODES) {
      setPageContentData(mode, HOST_DEVICE);
      assertEquals(getLabel(), HOST_DEVICE);
    }
    setPageContentData(settings.MultiDeviceSettingsMode.NO_HOST_SET, undefined);
    assertNotEquals(getLabel(), HOST_DEVICE);
  });

  test('changing host device and fixing mode changes header', function() {
    setPageContentData(
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED, HOST_DEVICE);
    assertEquals(getLabel(), HOST_DEVICE);
    const anotherHost = 'Super Duper ' + HOST_DEVICE;
    setPageContentData(
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED, anotherHost);
    assertEquals(getLabel(), anotherHost);
  });

  test('item is actionable if and only if a host is set', function() {
    setPageContentData(settings.MultiDeviceSettingsMode.NO_HOST_SET, undefined);
    assertFalse(
        multidevicePage.$$('#multidevice-item').hasAttribute('actionable'));
    for (let mode of HOST_SET_MODES) {
      setPageContentData(mode, HOST_DEVICE);
      assertTrue(
          multidevicePage.$$('#multidevice-item').hasAttribute('actionable'));
    }
  });

  test(
      'clicking item with verified host opens subpage with features',
      function() {
        setPageContentData(
            settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED, HOST_DEVICE);
        assertFalse(!!getSubpage());
        multidevicePage.$$('#multidevice-item').click();
        assertTrue(!!getSubpage());
        assertTrue(!!getSubpage().$$('settings-multidevice-feature-item'));
      });

  test(
      'clicking item with unverified set host opens subpage without features',
      function() {
        setPageContentData(
            settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
            HOST_DEVICE);
        assertFalse(!!getSubpage());
        multidevicePage.$$('#multidevice-item').click();
        assertTrue(!!getSubpage());
        assertFalse(!!getSubpage().$$('settings-multidevice-feature-item'));
      });
});
