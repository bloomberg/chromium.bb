// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('user_manager_test', function() {
  suite('UserManagerTest', function() {
    /** @type {?PrintPreviewUserManagerElement} */
    let userManager = null;

    /** @type {?print_preview.DestinationStore} */
    let destinationStore = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {?print_preview.CloudPrintInterface} */
    let cloudPrintInterface = null;

    const account1 = 'foo@chromium.org';
    const account2 = 'bar@chromium.org';

    /** @override */
    suiteSetup(function() {
      print_preview_test_utils.setupTestListenerElement();
    });

    /** @override */
    setup(function() {
      PolymerTest.clearBody();

      // Create data classes
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();
      // Set up a cloud printer for each account, so that search will work.
      cloudPrintInterface.setPrinter(
          print_preview_test_utils.getGoogleDriveDestination(account1));
      cloudPrintInterface.setPrinter(
          print_preview_test_utils.getGoogleDriveDestination(account2));

      userManager = document.createElement('print-preview-user-manager');

      // Initialize destination store.
      destinationStore = print_preview_test_utils.createDestinationStore();
      destinationStore.setCloudPrintInterface(cloudPrintInterface);
      const localDestinations = [];
      const destinations = print_preview_test_utils.getDestinations(
          nativeLayer, localDestinations);
      destinationStore.init(
          false /* isInAppKioskMode */, 'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */, []);
      nativeLayer.setLocalDestinations(localDestinations);

      // Set up user manager
      userManager.appKioskMode = false;
      userManager.destinationStore = destinationStore;
      userManager.invitationStore = new print_preview.InvitationStore();
      userManager.shouldReloadCookies = false;
      document.body.appendChild(userManager);
    });

    // Checks that initializing and updating user accounts works as expected.
    test('update users', function() {
      assertTrue(userManager.cloudPrintDisabled);

      userManager.setCloudPrintInterface(cloudPrintInterface);
      assertFalse(userManager.cloudPrintDisabled);
      assertEquals(undefined, userManager.activeUser);

      userManager.initUserAccounts([]);
      assertEquals('', userManager.activeUser);
      assertEquals(0, userManager.users.length);
      assertEquals(0, cloudPrintInterface.getCallCount('search'));

      // Simulate signing in and out of accounts. This should update the list of
      // users and the active user and triggers a call to search since the
      // destination store calls onDestinationsReload each time this event
      // fires.
      cr.webUIListenerCallback('user-accounts-updated', [account1]);
      assertEquals(account1, userManager.activeUser);
      assertEquals(1, userManager.users.length);
      assertEquals(1, cloudPrintInterface.getCallCount('search'));

      cr.webUIListenerCallback('user-accounts-updated', [account1, account2]);
      assertEquals(account1, userManager.activeUser);
      assertEquals(2, userManager.users.length);
      assertEquals(2, cloudPrintInterface.getCallCount('search'));

      cr.webUIListenerCallback('user-accounts-updated', [account2]);
      assertEquals(account2, userManager.activeUser);
      assertEquals(1, userManager.users.length);
      assertEquals(3, cloudPrintInterface.getCallCount('search'));

      cr.webUIListenerCallback('user-accounts-updated', []);
      assertEquals('', userManager.activeUser);
      assertEquals(0, userManager.users.length);
      assertEquals(4, cloudPrintInterface.getCallCount('search'));
    });

    test('update active user', function() {
      userManager.setCloudPrintInterface(cloudPrintInterface);
      userManager.initUserAccounts([account1, account2]);
      assertFalse(userManager.cloudPrintDisabled);
      assertEquals(account1, userManager.activeUser);
      assertEquals(2, userManager.users.length);

      // Don't call search at startup.
      assertEquals(0, cloudPrintInterface.getCallCount('search'));

      // Changing the active user doesn't result in a search call if
      // |shouldReloadCookies| is false, indicating the destinations aren't user
      // visible.
      userManager.updateActiveUser(account2);
      assertEquals(account2, userManager.activeUser);
      assertEquals(2, userManager.users.length);
      assertEquals(0, cloudPrintInterface.getCallCount('search'));

      // Changing the active user should result in a search call if
      // |shouldReloadCookies| is true.
      userManager.shouldReloadCookies = true;
      userManager.updateActiveUser(account1);
      return cloudPrintInterface.whenCalled('search').then(account => {
        assertEquals(account1, account);
        assertEquals(account1, userManager.activeUser);
        assertEquals(2, userManager.users.length);
        assertEquals(1, cloudPrintInterface.getCallCount('search'));
      });
    });
  });
});
