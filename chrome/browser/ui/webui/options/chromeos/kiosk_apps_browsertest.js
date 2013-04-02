// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for kiosk app settings WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function KioskAppSettingsWebUITest() {}

KioskAppSettingsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the kiosk app settings page.
   */
  browsePreload: 'chrome://settings-frame/kioskAppsOverlay',

  /**
   * Mock apps data.
   */
  apps_: [
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

  /**
   * Register a mock dictionary handler.
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler(
        ['getKioskApps',
         'addKioskApp',
         'removeKioskApp',
         'enableKioskAutoLaunch',
         'disableKioskAutoLaunch'
        ]);
    this.mockHandler.stubs().getKioskApps().
        will(callFunction(function() {
          KioskAppsOverlay.setApps(this.apps_);
        }.bind(this)));
    this.mockHandler.stubs().addKioskApp(ANYTHING);
    this.mockHandler.stubs().removeKioskApp(ANYTHING);
    this.mockHandler.stubs().enableKioskAutoLaunch(ANYTHING);
    this.mockHandler.stubs().disableKioskAutoLaunch(ANYTHING);
  }
};

// Test opening kiosk app settings has correct location and app items have
// correct label.
TEST_F('KioskAppSettingsWebUITest', 'testOpenKioskAppSettings', function() {
  assertEquals(this.browsePreload, document.location.href);

  var appItems = $('kiosk-app-list').items;
  assertEquals(this.apps_.length, appItems.length);
  assertEquals(this.apps_[0].name, appItems[0].name.textContent);
  assertFalse(appItems[0].icon.classList.contains('spinner'));
  assertEquals(this.apps_[1].id, appItems[1].name.textContent);
  assertTrue(appItems[1].icon.classList.contains('spinner'));
});

// Verify that enter key on 'kiosk-app-id-edit' adds an app.
TEST_F('KioskAppSettingsWebUITest', 'testAddKioskApp', function() {
  var testAppId = 'app_3';
  var appIdInput = $('kiosk-app-id-edit');

  appIdInput.value = testAppId;

  this.mockHandler.expects(once()).addKioskApp([testAppId]);
  var keypress = document.createEvent("KeyboardEvents");
  keypress.initKeyboardEvent('keypress', true, true, null, 'Enter', '');
  appIdInput.dispatchEvent(keypress);
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
  KioskAppsOverlay.updateApp(newApp2);

  assertEquals('app_2', appItems[1].data.id);
  expectEquals(newName, appItems[1].data.name, newName);
  expectEquals(newName, appItems[1].name.textContent);
  expectFalse(appItems[1].icon.classList.contains('spinner'));
  expectTrue(appItems[1].autoLaunch);
});

// Verify that showError makes error banner visible.
TEST_F('KioskAppSettingsWebUITest', 'testShowError', function() {
  KioskAppsOverlay.showError('A bad app');
  expectTrue($('kiosk-apps-error-banner').classList.contains('visible'));
});

// Verify that checking disable bailout checkbox brings up confirmation UI and
// the check only remains when the confirmation UI is acknowledged.
TEST_F('KioskAppSettingsWebUITest', 'testCheckDisableBailout', function() {
  var checkbox = $('kiosk-disable-bailout-shortcut');
  var confirmOverlay = KioskDisableBailoutConfirm.getInstance();
  expectFalse(confirmOverlay.visible);

  // Un-checking the box does not trigger confirmation.
  checkbox.checked = false;
  cr.dispatchSimpleEvent(checkbox, 'change');
  expectFalse(confirmOverlay.visible);

  // Checking the box trigger confirmation.
  checkbox.checked = true;
  cr.dispatchSimpleEvent(checkbox, 'change');
  expectTrue(confirmOverlay.visible);

  // Confirm it and the check remains.
  cr.dispatchSimpleEvent($('kiosk-disable-bailout-confirm-button'), 'click');
  expectTrue(checkbox.checked);
  expectFalse(confirmOverlay.visible);

  // And canceling resets the check.
  checkbox.checked = true;
  cr.dispatchSimpleEvent(checkbox, 'change');
  expectTrue(confirmOverlay.visible);
  cr.dispatchSimpleEvent($('kiosk-disable-bailout-cancel-button'), 'click');
  expectFalse(checkbox.checked);
  expectFalse(confirmOverlay.visible);
});
