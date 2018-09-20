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
      'setFeatureEnabledState',
    ]);
  }

  /** @override */
  showMultiDeviceSetupDialog() {
    this.methodCalled('showMultiDeviceSetupDialog');
  }

  /** @override */
  setFeatureEnabledState(feature, enabled, opt_authToken) {
    this.methodCalled(
        'setFeatureEnabledState', [feature, enabled, opt_authToken]);
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

  function setSuiteState(newState) {
    multidevicePage.pageContentData = Object.assign(
        {}, multidevicePage.pageContentData, {betterTogetherState: newState});
    Polymer.dom.flush();
  }

  function setSmartLockState(newState) {
    multidevicePage.pageContentData = Object.assign(
        {}, multidevicePage.pageContentData, {smartLockState: newState});
    Polymer.dom.flush();
  }

  /**
   * @param {!settings.MultiDeviceFeature} feature The feature to change.
   * @param {boolean} enabled Whether to enable or disable the feature.
   * @param {boolean} authRequired Whether authentication is required for the
   *     change.
   * @return {!Promise} Promise which resolves when the state change has been
   *     verified.
   * @private
   */
  function simulateFeatureStateChangeRequest(feature, enabled, authRequired) {
    // When the user requets a feature state change, an event with the relevant
    // details is handled.
    multidevicePage.fire(
        'feature-toggle-clicked', {feature: feature, enabled: enabled});
    Polymer.dom.flush();

    if (authRequired) {
      assertTrue(multidevicePage.showPasswordPromptDialog_);
      // Simulate the user entering a valid password, then closing the dialog.
      multidevicePage.fire('auth-token-changed', {value: 'validAuthToken'});
      // Simulate closing the password prompt dialog
      multidevicePage.$$('#multidevicePasswordPrompt').fire('close');
      Polymer.dom.flush();
    } else {
      assertFalse(multidevicePage.showPasswordPromptDialog_);
    }

    return browserProxy.whenCalled('setFeatureEnabledState').then(params => {
      assertEquals(feature, params[0]);
      assertEquals(enabled, params[1]);

      // Reset the resolver so that setFeatureEnabledState() can be called
      // multiple times in a test.
      browserProxy.resetResolver('setFeatureEnabledState');
    });
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

  test('policy prohibited suite shows policy indicator', function() {
    setPageContentData(settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
    assertFalse(!!multidevicePage.$$('cr-policy-indicator'));
    // Prohibit suite by policy.
    setSuiteState(settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertTrue(!!multidevicePage.$$('cr-policy-indicator'));
    // Reallow suite.
    setSuiteState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(!!multidevicePage.$$('cr-policy-indicator'));
  });

  test('Disabling features never requires authentication', () => {
    const Feature = settings.MultiDeviceFeature;

    const disableFeatureFn = feature => {
      return simulateFeatureStateChangeRequest(
          feature, false /* enabled */, false /* authRequired */);
    };

    return disableFeatureFn(Feature.BETTER_TOGETHER_SUITE)
        .then(() => {
          return disableFeatureFn(Feature.INSTANT_TETHERING);
        })
        .then(() => {
          return disableFeatureFn(Feature.MESSAGES);
        })
        .then(() => {
          return disableFeatureFn(Feature.SMART_LOCK);
        });
  });

  test('Enabling some features requires authentication; others do not', () => {
    const Feature = settings.MultiDeviceFeature;
    const FeatureState = settings.MultiDeviceFeatureState;

    const enableFeatureWithoutAuthFn = feature => {
      return simulateFeatureStateChangeRequest(
          feature, true /* enabled */, false /* authRequired */);
    };
    const enableFeatureWithAuthFn = feature => {
      return simulateFeatureStateChangeRequest(
          feature, true /* enabled */, true /* authRequired */);
    };

    // Start out with SmartLock being disabled by the user. This means that
    // the first attempt to enable BETTER_TOGETHER_SUITE below will not
    // require authentication.
    setSmartLockState(FeatureState.DISABLED_BY_USER);

    // INSTANT_TETHERING requires no authentication.
    return enableFeatureWithoutAuthFn(Feature.INSTANT_TETHERING)
        .then(() => {
          // MESSAGES requires no authentication.
          return enableFeatureWithoutAuthFn(Feature.MESSAGES);
        })
        .then(() => {
          // BETTER_TOGETHER_SUITE requires no authentication normally.
          return enableFeatureWithoutAuthFn(Feature.BETTER_TOGETHER_SUITE);
        })
        .then(() => {
          // BETTER_TOGETHER_SUITE requires authentication when SmartLock's
          // state is UNAVAILABLE_SUITE_DISABLED.
          setSmartLockState(FeatureState.UNAVAILABLE_SUITE_DISABLED);
          return enableFeatureWithAuthFn(Feature.BETTER_TOGETHER_SUITE);
        })
        .then(() => {
          // BETTER_TOGETHER_SUITE requires authentication when SmartLock's
          // state is UNAVAILABLE_INSUFFICIENT_SECURITY.
          setSmartLockState(FeatureState.UNAVAILABLE_INSUFFICIENT_SECURITY);
          return enableFeatureWithAuthFn(Feature.BETTER_TOGETHER_SUITE);
        })
        .then(() => {
          // SMART_LOCK always requires authentication.
          return enableFeatureWithAuthFn(Feature.SMART_LOCK);
        });
  });
});
