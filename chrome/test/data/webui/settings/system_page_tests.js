// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const {boolean} */
var HARDWARE_ACCELERATION_AT_STARTUP = true;

/**
 * @constructor
 * @extends {TestBrowserProxy}
 * @implements {settings.SystemPageBrowserProxy}
 */
function TestSystemPageBrowserProxy() {
  TestBrowserProxy.call(this, ['showProxySettings']);
}

TestSystemPageBrowserProxy.prototype = {
  __proto__: TestBrowserProxy.prototype,

  /** @override */
  showProxySettings: function() {
    this.methodCalled('showProxySettings');
  },

  /** @override */
  wasHardwareAccelerationEnabledAtStartup: function() {
    return HARDWARE_ACCELERATION_AT_STARTUP;
  },
};

suite('settings system page', function() {
  /** @type {TestSystemPageBrowserProxy} */
  var systemBrowserProxy;

  /** @type {settings.TestLifetimeBrowserProxy} */
  var lifetimeBrowserProxy;

  /** @type {SettingsSystemPageElement} */
  var systemPage;

  setup(function() {
    PolymerTest.clearBody();
    lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
    settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;
    systemBrowserProxy = new TestSystemPageBrowserProxy();
    settings.SystemPageBrowserProxyImpl.instance_ = systemBrowserProxy;

    systemPage = document.createElement('settings-system-page');
    systemPage.set('prefs', {
      background_mode: {
        enabled: {
          key: 'background_mode.enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
      hardware_acceleration_mode: {
        enabled: {
          key: 'hardware_acceleration_mode.enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: HARDWARE_ACCELERATION_AT_STARTUP,
        },
      },
      proxy: {
        key: 'proxy',
        type: chrome.settingsPrivate.PrefType.DICTIONARY,
        value: {mode: 'system'},
      },
    });
    document.body.appendChild(systemPage);
  });

  teardown(function() {
    systemPage.remove();
  });

  test('restart button', function() {
    var control = systemPage.$.hardwareAcceleration;
    expectEquals(HARDWARE_ACCELERATION_AT_STARTUP, control.checked);

    // Restart button should be hidden by default.
    expectFalse(!!control.querySelector('paper-button'));

    systemPage.set(
        'prefs.hardware_acceleration_mode.enabled.value',
        !HARDWARE_ACCELERATION_AT_STARTUP);
    Polymer.dom.flush();
    expectNotEquals(HARDWARE_ACCELERATION_AT_STARTUP, control.checked);

    var restart = control.querySelector('paper-button');
    expectTrue(!!restart);  // The "RESTART" button should be showing now.

    MockInteractions.tap(restart);
    return lifetimeBrowserProxy.whenCalled('restart');
  });

  test('proxy row', function() {
    MockInteractions.tap(systemPage.$.proxy);
    return systemBrowserProxy.whenCalled('showProxySettings');
  });

  test('proxy row enforcement', function() {
    var control = systemPage.$.proxy;
    var showProxyButton = control.querySelector('button');
    assertTrue(control.hasAttribute('actionable'));
    assertEquals(null, control.querySelector('cr-policy-pref-indicator'));
    assertFalse(showProxyButton.hidden);

    systemPage.set('prefs.proxy', {
      key: 'proxy',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {mode: 'system'},
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      extensionId: 'blah',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    Polymer.dom.flush();

    // The ability to show proxy settings should still be allowed when
    // extensions are installed.
    expectTrue(control.hasAttribute('actionable'));
    expectEquals(null, control.querySelector('cr-policy-pref-indicator'));
    expectFalse(showProxyButton.hidden);

    systemPage.set('prefs.proxy', {
      key: 'proxy',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {mode: 'system'},
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    Polymer.dom.flush();

    // When managed by policy directly, we disable the ability to show proxy
    // settings.
    expectFalse(control.hasAttribute('actionable'));
    expectNotEquals(null, control.querySelector('cr-policy-pref-indicator'));
    expectTrue(showProxyButton.hidden);
  });
});
