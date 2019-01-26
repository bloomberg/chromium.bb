// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_sync_controls', function() {
  suite('SyncControlsTest', function() {
    let syncControls = null;
    let browserProxy = null;

    /**
     * Returns sync prefs with everything synced.
     * @return {!settings.SyncPrefs}
     */
    function getSyncAllPrefs() {
      return {
        appsEnforced: false,
        appsRegistered: true,
        appsSynced: true,
        autofillEnforced: false,
        autofillRegistered: true,
        autofillSynced: true,
        bookmarksEnforced: false,
        bookmarksRegistered: true,
        bookmarksSynced: true,
        extensionsEnforced: false,
        extensionsRegistered: true,
        extensionsSynced: true,
        passwordsEnforced: false,
        passwordsRegistered: true,
        passwordsSynced: true,
        paymentsIntegrationEnabled: true,
        preferencesEnforced: false,
        preferencesRegistered: true,
        preferencesSynced: true,
        syncAllDataTypes: true,
        tabsEnforced: false,
        tabsRegistered: true,
        tabsSynced: true,
        themesEnforced: false,
        themesRegistered: true,
        themesSynced: true,
        typedUrlsEnforced: false,
        typedUrlsRegistered: true,
        typedUrlsSynced: true,
      };
    }

    setup(function() {
      browserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = browserProxy;

      PolymerTest.clearBody();
      syncControls = document.createElement('settings-sync-controls');
      document.body.appendChild(syncControls);

      // Start with Sync All.
      cr.webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
      Polymer.dom.flush();
    });

    teardown(function() {
      syncControls.remove();
    });

    test('SettingIndividualDatatypes', function() {
      const syncAllDataTypesControl = syncControls.$.syncAllDataTypesControl;
      assertFalse(syncAllDataTypesControl.disabled);
      assertTrue(syncAllDataTypesControl.checked);

      // Assert that all the individual datatype controls are disabled.
      const datatypeControls =
          syncControls.shadowRoot.querySelectorAll('.list-item cr-toggle');
      for (const control of datatypeControls) {
        assertTrue(control.disabled);
        assertTrue(control.checked);
      }

      // Uncheck the Sync All control.
      syncAllDataTypesControl.click();

      function verifyPrefs(prefs) {
        const expected = getSyncAllPrefs();
        expected.syncAllDataTypes = false;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        cr.webUIListenerCallback('sync-prefs-changed', expected);

        // Assert that all the individual datatype controls are enabled.
        for (const control of datatypeControls) {
          assertFalse(control.disabled);
          assertTrue(control.checked);
        }

        browserProxy.resetResolver('setSyncDatatypes');

        // Test an arbitrarily-selected control (extensions synced control).
        datatypeControls[2].click();
        return browserProxy.whenCalled('setSyncDatatypes')
            .then(function(prefs) {
              const expected = getSyncAllPrefs();
              expected.syncAllDataTypes = false;
              expected.extensionsSynced = false;
              assertEquals(JSON.stringify(expected), JSON.stringify(prefs));
            });
      }
      return browserProxy.whenCalled('setSyncDatatypes').then(verifyPrefs);
    });
  });

  suite('SyncControlsSubpageTest', function() {
    let syncControls = null;
    let browserProxy = null;

    setup(function() {
      browserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = browserProxy;

      PolymerTest.clearBody();

      syncControls = document.createElement('settings-sync-controls');
      settings.navigateTo(settings.routes.SYNC_ADVANCED);
      document.body.appendChild(syncControls);

      syncControls
          .syncStatus = {disabled: false, hasError: false, signedIn: true};
      Polymer.dom.flush();

      assertEquals(settings.routes.SYNC_ADVANCED, settings.getCurrentRoute());
    });

    teardown(function() {
      syncControls.remove();
    });

    test('SignedOut', function() {
      syncControls
          .syncStatus = {disabled: false, hasError: false, signedIn: false};
      assertEquals(settings.routes.SYNC, settings.getCurrentRoute());
    });

    test('PassphraseError', function() {
      syncControls.syncStatus = {
        disabled: false,
        hasError: true,
        signedIn: true,
        statusAction: settings.StatusAction.ENTER_PASSPHRASE
      };
      assertEquals(settings.routes.SYNC_ADVANCED, settings.getCurrentRoute());
    });

    test('SyncPaused', function() {
      syncControls.syncStatus = {
        disabled: false,
        hasError: true,
        signedIn: true,
        statusAction: settings.StatusAction.REAUTHENTICATE
      };
      assertEquals(settings.routes.SYNC, settings.getCurrentRoute());
    });
  });
});
