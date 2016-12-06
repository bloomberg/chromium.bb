// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "base/command_line.h"');

/**
 * TestFixture for kiosk app settings WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function KioskAppSettingsWebUITest() {}

KioskAppSettingsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the kiosk app settings page.
   */
  browsePreload: 'chrome://extensions-frame/',

  /** @override */
  commandLineSwitches: [{
    switchName: 'enable-consumer-kiosk',
  }],

  /**
   * Mock settings data.
   * @private
   */
  settings_: {
    apps: [
      {
        id: 'app_1',
        name: 'App1 Name',
        iconURL: '',
        autoLaunch: false,
        isLoading: false,
      },
      {
        id: 'app_2',
        name: '',  // no name
        iconURL: '',
        autoLaunch: false,
        isLoading: true,
      },
    ],
    disableBailout: false,
    hasAutoLaunchApp: false
  },

  /**
   * Register a mock dictionary handler.
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler(
        ['getKioskAppSettings',
         'addKioskApp',
         'removeKioskApp',
         'enableKioskAutoLaunch',
         'disableKioskAutoLaunch'
        ]);
    this.mockHandler.stubs().getKioskAppSettings().
        will(callFunction(function() {
          extensions.KioskAppsOverlay.setSettings(this.settings_);
        }.bind(this)));
    this.mockHandler.stubs().addKioskApp(ANYTHING);
    this.mockHandler.stubs().removeKioskApp(ANYTHING);
    this.mockHandler.stubs().enableKioskAutoLaunch(ANYTHING);
    this.mockHandler.stubs().disableKioskAutoLaunch(ANYTHING);
  },

  setUp: function() {
    // Shows the kiosk apps management overlay.
    cr.dispatchSimpleEvent($('add-kiosk-app'), 'click');
  }
};

// Test opening kiosk app settings has correct location and app items have
// correct label.
TEST_F('KioskAppSettingsWebUITest', 'testOpenKioskAppSettings', function() {
  assertEquals(this.browsePreload, document.location.href);

  var appItems = $('kiosk-app-list').items;
  assertEquals(this.settings_.apps.length, appItems.length);
  assertEquals(this.settings_.apps[0].name, appItems[0].name.textContent);
  assertFalse(appItems[0].icon.classList.contains('spinner'));
  assertEquals(this.settings_.apps[1].id, appItems[1].name.textContent);
  assertTrue(appItems[1].icon.classList.contains('spinner'));
});

// Verify that enter key on 'kiosk-app-id-edit' adds an app.
TEST_F('KioskAppSettingsWebUITest', 'testAddKioskApp', function() {
  var testAppId = 'app_3';
  var appIdInput = $('kiosk-app-id-edit');

  appIdInput.value = testAppId;

  this.mockHandler.expects(once()).addKioskApp([testAppId]);
  var keypress = new KeyboardEvent('keypress', {cancelable: true, bubbles: true, key: 'Enter'});
  appIdInput.dispatchEvent(keypress);
});

// Verify that the 'kiosk-app-add' button adds an app.
TEST_F('KioskAppSettingsWebUITest', 'testAddKioskAppByAddButton', function() {
  var testAppId = 'app_3';
  $('kiosk-app-id-edit').value = testAppId;

  this.mockHandler.expects(once()).addKioskApp([testAppId]);
  cr.dispatchSimpleEvent($('kiosk-app-add'), 'click');
});

// Verify that the 'done' button adds an app.
TEST_F('KioskAppSettingsWebUITest', 'testAddKioskAppByDoneButton', function() {
  var testAppId = 'app_3';
  $('kiosk-app-id-edit').value = testAppId;

  this.mockHandler.expects(once()).addKioskApp([testAppId]);
  cr.dispatchSimpleEvent($('kiosk-options-overlay-confirm'), 'click');
});

// Test the row delete button.
TEST_F('KioskAppSettingsWebUITest', 'testRemoveKioskApp', function() {
  var appItem = $('kiosk-app-list').items[0];
  var appId = appItem.data.id;

  this.mockHandler.expects(once()).removeKioskApp([appId]);
  appItem.querySelector('.row-delete-button').click();
});

// Test enable/disable auto launch buttons.
TEST_F('KioskAppSettingsWebUITest', 'testEnableDisableAutoLaunch', function() {
  var appItem = $('kiosk-app-list').items[0];
  var appId = appItem.data.id;

  var enableAutoLaunchCalled = false;
  this.mockHandler.expects(once()).enableKioskAutoLaunch([appId]).
      will(callFunction(function() {
        enableAutoLaunchCalled = true;
      }));
  appItem.querySelector('.enable-auto-launch-button').click();
  expectTrue(enableAutoLaunchCalled);

  var disableAutoLaunchCalled = false;
  this.mockHandler.expects(once()).disableKioskAutoLaunch([appId]).
      will(callFunction(function() {
        disableAutoLaunchCalled = true;
      }));
  appItem.querySelector('.disable-auto-launch-button').click();
  expectTrue(disableAutoLaunchCalled);
});

// Verify that updateApp updates app info.
TEST_F('KioskAppSettingsWebUITest', 'testUpdateApp', function() {
  var appItems = $('kiosk-app-list').items;
  assertEquals(appItems[1].data.id, 'app_2');
  expectEquals(appItems[1].data.name, '');
  expectTrue(appItems[1].icon.classList.contains('spinner'));
  expectFalse(appItems[1].autoLaunch);

  // New data changes name, autoLaunch and isLoading.
  var newName = 'Name for App2';
  var newApp2 = {
    id: 'app_2',
    name: newName,
    iconURL: '',
    autoLaunch: true,
    isLoading: false,
  };
  extensions.KioskAppsOverlay.updateApp(newApp2);

  assertEquals('app_2', appItems[1].data.id);
  expectEquals(newName, appItems[1].data.name, newName);
  expectEquals(newName, appItems[1].name.textContent);
  expectFalse(appItems[1].icon.classList.contains('spinner'));
  expectTrue(appItems[1].autoLaunch);
});

// Verify that showError makes error banner visible.
TEST_F('KioskAppSettingsWebUITest', 'testShowError', function() {
  extensions.KioskAppsOverlay.showError('A bad app');
  expectTrue($('kiosk-apps-error-banner').classList.contains('visible'));
});

// Verify that checking disable bailout checkbox brings up confirmation UI and
// the check only remains when the confirmation UI is acknowledged.
TEST_F('KioskAppSettingsWebUITest', 'testCheckDisableBailout', function() {
  var checkbox = $('kiosk-disable-bailout-shortcut');
  var confirmOverlay = $('kiosk-disable-bailout-confirm-overlay');
  expectFalse(confirmOverlay.classList.contains('showing'));

  // Un-checking the box does not trigger confirmation.
  checkbox.checked = false;
  cr.dispatchSimpleEvent(checkbox, 'change');
  expectFalse(confirmOverlay.classList.contains('showing'));

  // Checking the box trigger confirmation.
  checkbox.checked = true;
  cr.dispatchSimpleEvent(checkbox, 'change');
  expectTrue(confirmOverlay.classList.contains('showing'));

  // Confirm it and the check remains.
  cr.dispatchSimpleEvent($('kiosk-disable-bailout-confirm-button'), 'click');
  expectTrue(checkbox.checked);
  expectFalse(confirmOverlay.classList.contains('showing'));

  // And canceling resets the check.
  checkbox.checked = true;
  cr.dispatchSimpleEvent(checkbox, 'change');
  expectTrue(confirmOverlay.classList.contains('showing'));
  cr.dispatchSimpleEvent($('kiosk-disable-bailout-cancel-button'), 'click');
  expectFalse(checkbox.checked);
  expectFalse(confirmOverlay.classList.contains('showing'));
});

// Verify that disable bailout checkbox is hidden without kiosk auto launch.
TEST_F('KioskAppSettingsWebUITest', 'testHideDisableBailout', function() {
  var checkbox = $('kiosk-disable-bailout-shortcut');
  var kioskEnabledSettings = {
    kioskEnabled: true,
    autoLaunchEnabled: true
  };
  extensions.KioskAppsOverlay.enableKiosk(kioskEnabledSettings);
  expectFalse(checkbox.parentNode.hidden);

  kioskEnabledSettings.autoLaunchEnabled = false;
  extensions.KioskAppsOverlay.enableKiosk(kioskEnabledSettings);
  expectTrue(checkbox.parentNode.hidden);
});

// Verify that disable bailout checkbox is disabled with no auto launch app.
TEST_F('KioskAppSettingsWebUITest', 'testAllowDisableBailout', function() {
  var checkbox = $('kiosk-disable-bailout-shortcut');

  this.settings_.hasAutoLaunchApp = false;
  extensions.KioskAppsOverlay.setSettings(this.settings_);
  expectTrue(checkbox.disabled);

  this.settings_.hasAutoLaunchApp = true;
  extensions.KioskAppsOverlay.setSettings(this.settings_);
  expectFalse(checkbox.disabled);
});

/**
 * TestFixture for kiosk app settings when consumer kiosk is disabled.
 * @extends {testing.Test}
 * @constructor
 */
function NoConsumerKioskWebUITest() {}

NoConsumerKioskWebUITest.prototype = {
  __proto__: KioskAppSettingsWebUITest.prototype,

  /** @override */
  commandLineSwitches: [],

  /** @override */
  setUp: function() {}
};

// Test kiosk app settings are not visible when consumer kiosk is disabled.
TEST_F('NoConsumerKioskWebUITest', 'settingsHidden', function() {
  assertEquals(this.browsePreload, document.location.href);
  assertTrue($('add-kiosk-app').hidden);
});
