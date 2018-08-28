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
   * @param {!settings.MultiDeviceSettingsMode} mode
   * @param {boolean} isSuiteSupported
   * @return {!MultiDevicePageContentData}
   */
  function getFakePageContentData(mode, isSuiteSupported) {
    return {
      mode: mode,
      hostDeviceName: HOST_SET_MODES.includes(mode) ? 'Pixel XL' : undefined,
      betterTogetherState: isSuiteSupported ?
          settings.MultiDeviceFeatureState.ENABLED_BY_USER :
          settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
    };
  }

  /**
   * @param {!settings.MultiDeviceSettingsMode} newMode
   * @param {boolean} isSuiteSupported
   */
  function changePageContent(newMode, isSuiteSupported) {
    cr.webUIListenerCallback(
        'settings.updateMultidevicePageContentData',
        getFakePageContentData(newMode, isSuiteSupported));
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

  // Initializes page container with provided mode and sets the Better Together
  // suite's feature state based on provided boolean. It returns a promise that
  // only resolves when the multidevice-page has changed its pageContentData and
  // the ensuing callbacks back been flushed.
  /**
   * @param {!settings.MultiDeviceSettingsMode} mode
   * @param {boolean} isSuiteSupported
   * @return {!Promise}
   */
  function setInitialPageContent(mode, isSuiteSupported) {
    if (multidevicePageContainer)
      multidevicePageContainer.remove();
    browserProxy = new TestMultideviceBrowserProxy(
        getFakePageContentData(mode, isSuiteSupported));
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    multidevicePageContainer =
        document.createElement('settings-multidevice-page-container');
    document.body.appendChild(multidevicePageContainer);

    return browserProxy.whenCalled('getPageContentData')
        .then(Polymer.dom.flush());
  }

  test('WebUIListener toggles multidevice page based on mode', function() {
    return setInitialPageContent(
               settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS,
               /* isSuiteSupported */ true)
        .then(() => {
          // Check that the settings-page is visible iff the mode is not
          // NO_ELIGIBLE_HOSTS.
          for (let mode of ALL_MODES) {
            changePageContent(mode, /* isSuiteSupported */ true);
            assertEquals(
                !getMultidevicePage(),
                mode === settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
          }
          // One more loop to ensure we transition in and out of
          // NO_ELIGIBLE_HOSTS mode.
          for (let mode of ALL_MODES) {
            changePageContent(mode, /* isSuiteSupported */ true);
            assertEquals(
                !getMultidevicePage(),
                mode === settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
          }
        });
  });

  test(
      'WebUIListener toggles multidevice page based suite support', function() {
        return setInitialPageContent(
                   settings.MultiDeviceSettingsMode.NO_HOST_SET,
                   /* isSuiteSupported */ true)
            .then(() => {
              // Check that the settings-page is visible iff the suite is
              // supported.
              assertTrue(!!getMultidevicePage());
              changePageContent(
                  settings.MultiDeviceSettingsMode.NO_HOST_SET,
                  /* isSuiteSupported */ false);
              assertFalse(!!getMultidevicePage());
              changePageContent(
                  settings.MultiDeviceSettingsMode.NO_HOST_SET,
                  /* isSuiteSupported */ true);
              assertTrue(!!getMultidevicePage());
            });
      });

  test('multidevice-page is not attached if no host is found', function() {
    return setInitialPageContent(
               settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS,
               /* isSuiteSupported */ true)
        .then(() => assertFalse(!!getMultidevicePage()));
  });

  test('multidevice-page is attached if a potential host is found', function() {
    return setInitialPageContent(
               settings.MultiDeviceSettingsMode.NO_HOST_SET,
               /* isSuiteSupported */ true)
        .then(() => assertTrue(!!getMultidevicePage()));
  });

  test(
      'multidevice-page is not attached if suite is not supported', function() {
        return setInitialPageContent(
                   settings.MultiDeviceSettingsMode.NO_HOST_SET,
                   /* isSuiteSupported */ true)
            .then(() => {
              for (let mode of ALL_MODES) {
                changePageContent(mode, /* isSuiteSupported */ false);
                assertFalse(!!getMultidevicePage());
              }
            });
      });

  test('multidevice-page is attached if a set host is found', function() {
    return setInitialPageContent(
               settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
               /* isSuiteSupported */ true)
        .then(() => assertTrue(!!getMultidevicePage()));
  });

  test(
      'pageContentData property passes to multidevice page if present',
      function() {
        return setInitialPageContent(
                   settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS,
                   /* isSuiteSupported */ true)
            .then(() => {
              for (let mode of ALL_MODES) {
                changePageContent(mode, /* isSuiteSupported */ true);
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
