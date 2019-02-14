// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-pwa-permission-view>', function() {
  let pwaPermissionView;
  let fakeHandler;

  const TEST_APP_ID = '1';

  function getToggleFromPermissionItem(permissionItemId) {
    return pwaPermissionView.root.getElementById(permissionItemId)
        .root.querySelector('app-management-permission-toggle')
        .root.querySelector('cr-toggle');
  }

  function getPermissionBoolByType(permissionType) {
    return app_management.util.getPermissionValueBool(
        pwaPermissionView.app_, permissionType);
  }

  async function clickToggle(permissionItemId) {
    getToggleFromPermissionItem(permissionItemId).click();
    await fakeHandler.flushForTesting();
  }

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Add an app, and make it the currently selected app.
    await fakeHandler.addApp(TEST_APP_ID);
    app_management.Store.getInstance().dispatch(
        app_management.actions.changePage(PageType.DETAIL, TEST_APP_ID));
    pwaPermissionView =
        document.createElement('app-management-pwa-permission-view');
    replaceBody(pwaPermissionView);
  });

  test('App is rendered correctly', function() {
    assertEquals(
        app_management.Store.getInstance().data.currentPage.selectedAppId,
        pwaPermissionView.app_.id);
  });

  test(
      'Clicking the permission toggle changes the permission of the app',
      async function() {
        let checkToggle = async function(permissionType, permissionId) {
          assertTrue(getPermissionBoolByType(permissionType));
          assertTrue(getToggleFromPermissionItem(permissionId).checked);
          await clickToggle(permissionId);
          assertFalse(getPermissionBoolByType(permissionType));
          assertFalse(getToggleFromPermissionItem(permissionId).checked);
        };

        await checkToggle(
            'CONTENT_SETTINGS_TYPE_NOTIFICATIONS', 'notifications');
        await checkToggle('CONTENT_SETTINGS_TYPE_GEOLOCATION', 'location');
        await checkToggle('CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA', 'camera');
        await checkToggle(
            'CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC', 'microphone');
      });
});
