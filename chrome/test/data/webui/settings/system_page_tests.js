// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_system_page', function() {
  /** @const {boolean} */
  var HARDWARE_ACCELERATION_AT_STARTUP = true;

  /**
   * @constructor
   * @extends {TestBrowserProxy}
   * @implements {settings.SystemPageBrowserProxy}
   */
  function TestSystemPageBrowserProxy() {
    settings.TestBrowserProxy.call(this, []);
  }

  TestSystemPageBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    changeProxySettings: assertNotReached,

    /** @override */
    wasHardwareAccelerationEnabledAtStartup: function() {
      return HARDWARE_ACCELERATION_AT_STARTUP;
    },
  };

  suite('SettingsDevicePage', function() {
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
      settings.SystemPageBrowserProxyImpl.instance_ =
          new TestSystemPageBrowserProxy();

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
      });
      document.body.appendChild(systemPage);
    });

    teardown(function() { systemPage.remove(); });

    test('restart button', function() {
      var control = systemPage.$.hardwareAcceleration;
      expectEquals(control.checked, HARDWARE_ACCELERATION_AT_STARTUP);

      // Restart button should be hidden by default.
      expectFalse(!!control.querySelector('paper-button'));

      systemPage.set('prefs.hardware_acceleration_mode.enabled.value',
                     !HARDWARE_ACCELERATION_AT_STARTUP);
      Polymer.dom.flush();
      expectNotEquals(control.checked, HARDWARE_ACCELERATION_AT_STARTUP);

      var restart = control.querySelector('paper-button');
      expectTrue(!!restart);  // The "RESTART" button should be showing now.

      MockInteractions.tap(restart);
      return lifetimeBrowserProxy.whenCalled('restart');
    });
  });
});
