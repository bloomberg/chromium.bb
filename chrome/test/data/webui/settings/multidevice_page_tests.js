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
  let ALL_MODES;
  const HOST_DEVICE = 'Pixel XL';

  /**
   * Sets pageContentData to the specified mode. If it is a mode corresponding
   * to a set host, it will set the hostDeviceName to the provided name or else
   * default to HOST_DEVICE.
   * @param {settings.MultiDeviceSettingsMode} newMode
   * @param {string|undefined} newHostDeviceName Overrides default if there
   *     newMode corresponds to a set host.
   */
  function setPageContentData(newMode, newHostDeviceName) {
    let newPageContentData = {mode: newMode};
    if ([
          settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
          settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
          settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
        ].includes(newMode)) {
      newPageContentData.hostDeviceName = newHostDeviceName || HOST_DEVICE;
    }
    multidevicePage.pageContentData = newPageContentData;
    Polymer.dom.flush();
  }

  suiteSetup(function() {
    ALL_MODES = Object.values(settings.MultiDeviceSettingsMode);
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
    setPageContentData(settings.MultiDeviceSettingsMode.NO_HOST_SET);
    const button = multidevicePage.$$('paper-button');
    assertTrue(!!button);
    button.click();
    return browserProxy.whenCalled('showMultiDeviceSetupDialog');
  });

  test('headings render based on mode and host', function() {
    for (const mode of ALL_MODES) {
      setPageContentData(mode);
      assertEquals(multidevicePage.isHostSet(), getLabel() === HOST_DEVICE);
    }
  });

  test('changing host device changes header', function() {
    setPageContentData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertEquals(getLabel(), HOST_DEVICE);
    const anotherHost = 'Super Duper ' + HOST_DEVICE;
    setPageContentData(
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED, anotherHost);
    assertEquals(getLabel(), anotherHost);
  });

  test('item is actionable if and only if a host is set', function() {
    for (const mode of ALL_MODES) {
      setPageContentData(mode);
      assertEquals(
          multidevicePage.isHostSet(),
          !!multidevicePage.$$('#multidevice-item').hasAttribute('actionable'));
    }
  });

  test(
      'clicking item with verified host opens subpage with features',
      function() {
        setPageContentData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
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
