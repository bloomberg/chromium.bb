// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_kerberos_accounts', function() {
  /** @implements {settings.KerberosAccountsBrowserProxy} */
  class TestKerberosAccountsBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'getAccounts',
        'addAccount',
        'reauthenticateAccount',
        'removeAccount',
      ]);
    }

    /** @override */
    getAccounts() {
      this.methodCalled('getAccounts');

      return Promise.resolve([
        {
          principalName: 'user@REALM',
          isSignedIn: true,
          pic: 'pic',
        },
        {
          principalName: 'user2@REALM2',
          isSignedIn: false,
          pic: 'pic2',
        }
      ]);
    }

    /** @override */
    addAccount() {
      this.methodCalled('addAccount');
    }

    /** @override */
    reauthenticateAccount(principalName) {
      this.methodCalled('reauthenticateAccount', principalName);
    }

    /** @override */
    removeAccount(account) {
      this.methodCalled('removeAccount', account);
    }
  }

  suite('KerberosAccountsTests', function() {
    let browserProxy = null;
    let kerberosAccounts = null;
    let accountList = null;

    setup(function() {
      browserProxy = new TestKerberosAccountsBrowserProxy();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();

      kerberosAccounts = document.createElement('settings-kerberos-accounts');
      document.body.appendChild(kerberosAccounts);
      accountList = kerberosAccounts.$$('#account-list');
      assertTrue(!!accountList);

      settings.navigateTo(settings.routes.KERBEROS_ACCOUNTS);
    });

    teardown(function() {
      kerberosAccounts.remove();
    });

    test('AccountListIsPopulatedAtStartup', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        // 2 accounts were added in |getAccounts()| mock above.
        assertEquals(2, accountList.items.length);
      });
    });

    test('AddAccount', function() {
      assertFalse(kerberosAccounts.$$('#add-account-button').disabled);
      kerberosAccounts.$$('#add-account-button').click();
      assertEquals(1, browserProxy.getCallCount('addAccount'));
    });

    test('ReauthenticateAccount', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        // Note that both accounts have a reauth button, but [0] is hidden, so
        // click [1] (clicking a hidden button works, but it feels weird).
        kerberosAccounts.root.querySelectorAll('.reauth-button')[1].click();
        assertEquals(1, browserProxy.getCallCount('reauthenticateAccount'));
        return browserProxy.whenCalled('reauthenticateAccount')
            .then((principalName) => {
              assertEquals('user2@REALM2', principalName);
            });
      });
    });

    test('RemoveAccount', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        // Click on 'More Actions' for the first account.
        kerberosAccounts.root.querySelectorAll('cr-icon-button')[0].click();
        // Click on 'Remove account'.
        kerberosAccounts.$$('cr-action-menu').querySelector('button').click();

        return browserProxy.whenCalled('removeAccount').then((account) => {
          assertEquals('user@REALM', account.principalName);
        });
      });
    });

    test('AccountListIsUpdatedWhenKerberosAccountsUpdates', function() {
      assertEquals(1, browserProxy.getCallCount('getAccounts'));
      cr.webUIListenerCallback('kerberos-accounts-changed');
      assertEquals(2, browserProxy.getCallCount('getAccounts'));
    });
  });
});
