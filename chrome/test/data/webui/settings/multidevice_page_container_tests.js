// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {settings.MultideviceBrowserProxy}
 * Note: showMultiDeviceSetupDialog is not used by the
 * multidevice-page-container element.
 */
class TestMultideviceBrowserProxy extends TestBrowserProxy {
  constructor(initialPageContentData) {
    super([
      'showMultiDeviceSetupDialog',
      'getPageContentData',
    ]);
    this.data = initialPageContentData;
  }

  /** @override */
  getPageContentData() {
    this.methodCalled('getPageContentData');
    return Promise.resolve(this.data);
  }
}

suite('Multidevice', function() {
  let multidevicePageContainer = null;
  let browserProxy = null;
  let ALL_MODES;
  let HOST_SET_MODES;

  /**
   * @param {!settings.MultiDeviceSettingsMode}
   * @return {!MultiDevicePageContentData}
   */
  function getFakePageContentData(mode) {
    return {
      mode: mode,
      hostDeviceName: HOST_SET_MODES.includes(mode) ? 'Pixel XL' : undefined,
    };
  }

  /**
   * @param {!settings.MultiDeviceSettingsMode}
   */
  function changeHostStatus(newMode) {
    cr.webUIListenerCallback(
        'settings.updateMultidevicePageContentData',
        getFakePageContentData(newMode));
    Polymer.dom.flush();
  }

  suiteSetup(function() {
    ALL_MODES = Object.values(settings.MultiDeviceSettingsMode);
    HOST_SET_MODES = [
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ];
  });

  /** @return {?HTMLElement} */
  const getMultidevicePage = () =>
      multidevicePageContainer.$$('settings-multidevice-page');

  // Initializes page container with provided mode and returns a promise that
  // only resolves when the multidevice-page has changed its pageContentData and
  // the ensuing callbacks back been flushed.
  /**
   * @param {!settings.MultiDeviceSettingsMode}
   * @return {!Promise}
   */
  function setInitialHostStatus(mode) {
    if (multidevicePageContainer)
      multidevicePageContainer.remove();
    browserProxy =
        new TestMultideviceBrowserProxy(getFakePageContentData(mode));
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    multidevicePageContainer =
        document.createElement('settings-multidevice-page-container');
    assertTrue(!!multidevicePageContainer);

    document.body.appendChild(multidevicePageContainer);

    return browserProxy.whenCalled('getPageContentData')
        .then(Polymer.dom.flush());
  }

  test('WebUIListener toggles multidevice page', function() {
    return setInitialHostStatus(
               settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS)
        .then(() => {
          // Check that the settings-page is visible iff the mode is not
          // NO_ELIGIBLE_HOSTS.
          for (let mode of ALL_MODES) {
            changeHostStatus(mode);
            assertEquals(
                !getMultidevicePage(),
                mode === settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
          }
          // One more loop to ensure we transition in and out of
          // NO_ELIGIBLE_HOSTS mode.
          for (let mode of ALL_MODES) {
            changeHostStatus(mode);
            assertEquals(
                !getMultidevicePage(),
                mode === settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
          }
        });
  });

  test('multidevice-page is not attached if not host is found', function() {
    return setInitialHostStatus(
               settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS)
        .then(() => assertTrue(!getMultidevicePage()));
  });

  test('multidevice-page is attached if a potential host is found', function() {
    return setInitialHostStatus(settings.MultiDeviceSettingsMode.NO_HOST_SET)
        .then(() => assertTrue(!!getMultidevicePage()));
  });

  test('multidevice-page is attached if a set host is found', function() {
    return setInitialHostStatus(
               settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED)
        .then(() => assertTrue(!!getMultidevicePage()));
  });

  test(
      'pageContentData property passes to multidevice page if present',
      function() {
        return setInitialHostStatus(
                   settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS)
            .then(() => {
              for (let mode of ALL_MODES) {
                changeHostStatus(mode);
                if (mode === settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS)
                  assertEquals(getMultidevicePage(), null);
                else
                  assertDeepEquals(
                      multidevicePageContainer.pageContentData,
                      getMultidevicePage().pageContentData);
              }
            });
      });
});
