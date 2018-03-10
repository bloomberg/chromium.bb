// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_sync_account_control', function() {
  /**
   * @param {!Element} element
   * @param {boolean} displayed
   */
  function assertVisible(element, displayed) {
    assertEquals(
        displayed, window.getComputedStyle(element)['display'] != 'none');
  }

  suite('SyncAccountControl', function() {
    let peoplePage = null;
    let browserProxy = null;
    let testElement = null;

    /**
     * @param {number} count
     * @param {boolean} signedIn
     */
    function forcePromoResetWithCount(count, signedIn) {
      browserProxy.setImpressionCount(count);
      // Flipping syncStatus.signedIn will force promo state to be reset.
      sync_test_util.simulateSyncStatus({
        signedIn: !signedIn,
      });
      sync_test_util.simulateSyncStatus({
        signedIn: signedIn,
      });
    }

    suiteSetup(function() {
      // Force UIs to think DICE is enabled for this profile.
      loadTimeData.overrideValues({
        diceEnabled: true,
      });
    });

    setup(function() {
      browserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = browserProxy;

      PolymerTest.clearBody();
      testElement = document.createElement('settings-sync-account-control');
      document.body.appendChild(testElement);

      Polymer.dom.flush();

      return Promise.all([
        browserProxy.whenCalled('getSyncStatus'),
        browserProxy.whenCalled('getStoredAccounts'),
      ]);
    });

    teardown(function() {
      testElement.remove();
    });

    test('promo shows/hides in the right states', function() {
      // Not signed in, no accounts, will show banner.
      sync_test_util.simulateStoredAccounts([]);
      forcePromoResetWithCount(0, false);
      const banner = testElement.$$('#banner');
      assertVisible(banner, true);
      // Flipping signedIn in forcePromoResetWithCount should increment count.
      return browserProxy.whenCalled('incrementPromoImpressionCount')
          .then(() => {
            forcePromoResetWithCount(
                settings.MAX_SIGNIN_PROMO_IMPRESSION + 1, false);
            assertVisible(banner, false);

            // Not signed in, has accounts, will show banner.
            sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);
            forcePromoResetWithCount(0, false);
            assertVisible(banner, true);
            forcePromoResetWithCount(
                settings.MAX_SIGNIN_PROMO_IMPRESSION + 1, false);
            assertVisible(banner, false);

            // signed in, banners never show.
            sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);
            forcePromoResetWithCount(0, true);
            assertVisible(banner, false);
            forcePromoResetWithCount(
                settings.MAX_SIGNIN_PROMO_IMPRESSION + 1, true);
            assertVisible(banner, false);
          });
    });

    test('not signed in and no stored accounts', function() {
      sync_test_util.simulateSyncStatus(
          {signedIn: false, signedInUsername: ''});
      sync_test_util.simulateStoredAccounts([]);

      assertVisible(testElement.$$('#promo-headers'), true);
      assertVisible(testElement.$$('#avatar-row'), false);
      assertVisible(testElement.$$('#menu'), false);
      assertVisible(testElement.$$('#sign-in'), true);

      testElement.$$('#sign-in').click();
      return browserProxy.whenCalled('startSignIn');
    });

    test('not signed in but has stored accounts', function() {
      sync_test_util.simulateSyncStatus(
          {signedIn: false, signedInUsername: ''});
      sync_test_util.simulateStoredAccounts([
        {
          fullName: 'fooName',
          givenName: 'foo',
          email: 'foo@foo.com',
        },
        {
          fullName: 'barName',
          givenName: 'bar',
          email: 'bar@bar.com',
        },
      ]);

      const userInfo = testElement.$$('#user-info');
      const syncButton = testElement.$$('#avatar-row .action-button');

      // Avatar row shows the right account.
      assertVisible(testElement.$$('#promo-headers'), true);
      assertVisible(testElement.$$('#avatar-row'), true);
      assertTrue(userInfo.textContent.includes('fooName'));
      assertTrue(userInfo.textContent.includes('foo@foo.com'));
      assertFalse(userInfo.textContent.includes('barName'));
      assertFalse(userInfo.textContent.includes('bar@bar.com'));

      // Menu contains the right items.
      assertTrue(!!testElement.$$('#menu'));
      assertFalse(testElement.$$('#menu').open);
      const items = testElement.root.querySelectorAll('.dropdown-item');
      assertEquals(3, items.length);
      assertTrue(items[0].textContent.includes('foo@foo.com'));
      assertTrue(items[1].textContent.includes('bar@bar.com'));
      assertEquals(items[2].id, 'sign-in-item');

      // "sync to" button is showing the correct name and syncs with the
      // correct account when clicked.
      assertVisible(syncButton, true);
      assertVisible(testElement.$$('#avatar-row .secondary-button'), false);
      assertTrue(syncButton.textContent.includes('foo'));
      assertFalse(syncButton.textContent.includes('bar'));
      syncButton.click();
      Polymer.dom.flush();

      return browserProxy.whenCalled('startSyncingWithEmail')
          .then(email => {
            assertEquals(email, 'foo@foo.com');

            assertVisible(testElement.$$('#dropdown-arrow'), true);
            assertFalse(
                testElement.$$('#sync-icon-container').hasAttribute('syncing'));

            testElement.$$('#dropdown-arrow').click();
            Polymer.dom.flush();
            assertTrue(testElement.$$('#menu').open);

            // Switching selected account will update UI with the right name and
            // email.
            items[1].click();
            Polymer.dom.flush();
            assertFalse(userInfo.textContent.includes('fooName'));
            assertFalse(userInfo.textContent.includes('foo@foo.com'));
            assertTrue(userInfo.textContent.includes('barName'));
            assertTrue(userInfo.textContent.includes('bar@bar.com'));
            assertVisible(syncButton, true);
            assertTrue(syncButton.textContent.includes('bar'));
            assertFalse(syncButton.textContent.includes('foo'));

            browserProxy.resetResolver('startSyncingWithEmail');
            syncButton.click();
            Polymer.dom.flush();

            return browserProxy.whenCalled('startSyncingWithEmail');
          })
          .then(email => {
            assertEquals(email, 'bar@bar.com');

            // Tapping the last menu item will initiate sign-in.
            items[2].click();
            return browserProxy.whenCalled('startSignIn');
          });
    });

    test('signed in', function() {
      sync_test_util.simulateSyncStatus(
          {signedIn: true, signedInUsername: 'bar@bar.com'});
      sync_test_util.simulateStoredAccounts([
        {
          fullName: 'fooName',
          givenName: 'foo',
          email: 'foo@foo.com',
        },
        {
          fullName: 'barName',
          givenName: 'bar',
          email: 'bar@bar.com',
        },
      ]);

      assertVisible(testElement.$$('#avatar-row'), true);
      assertVisible(testElement.$$('#dropdown-arrow'), false);
      assertVisible(testElement.$$('#promo-headers'), false);
      assertTrue(
          testElement.$$('#sync-icon-container').hasAttribute('syncing'));
      assertFalse(!!testElement.$$('#menu'));

      const userInfo = testElement.$$('#user-info');
      assertTrue(userInfo.textContent.includes('barName'));
      assertTrue(userInfo.textContent.includes('bar@bar.com'));
      assertFalse(userInfo.textContent.includes('fooName'));
      assertFalse(userInfo.textContent.includes('foo@foo.com'));

      assertVisible(testElement.$$('#avatar-row .action-button'), false);
      assertVisible(testElement.$$('#avatar-row .secondary-button'), true);

      testElement.$$('#avatar-row .secondary-button').click();
      Polymer.dom.flush();

      assertEquals(settings.getCurrentRoute(), settings.routes.SIGN_OUT);
    });
  });
});
