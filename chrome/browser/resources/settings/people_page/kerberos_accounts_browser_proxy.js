// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Kerberos Accounts" subsection of
 * the "People" section of Settings, to interact with the browser. Chrome OS
 * only.
 */
cr.exportPath('settings');

/**
 * Information for a Chrome OS Kerberos account.
 * @typedef {{
 *   principalName: string,
 *   isSignedIn: boolean,
 *   pic: string,
 * }}
 */
settings.KerberosAccount;

cr.define('settings', function() {
  /** @interface */
  class KerberosAccountsBrowserProxy {
    /**
     * Returns a Promise for the list of Kerberos accounts held in the kerberosd
     * system daemon.
     * @return {!Promise<!Array<!settings.KerberosAccount>>}
     */
    getAccounts() {}

    /**
     * Triggers the 'Add account' flow.
     */
    addAccount() {}

    /**
     * Triggers the re-authentication flow for the account pointed to by
     * |principalName|.
     * @param {!string} principalName
     */
    reauthenticateAccount(principalName) {}

    /**
     * Removes |account| from the set of Kerberos accounts.
     * @param {!settings.KerberosAccount} account
     */
    removeAccount(account) {}
  }

  /**
   * @implements {settings.KerberosAccountsBrowserProxy}
   */
  class KerberosAccountsBrowserProxyImpl {
    /** @override */
    getAccounts() {
      // TODO(https://crbug.com/952232): Implement
      return new Promise(function(resolve, reject) {
        resolve([
          {
            principalName: 'user@REALM',
            isSignedIn: true,
            pic: 'chrome://theme/IDR_LOGIN_DEFAULT_USER_2',
          },
          {
            principalName: 'user2@REALM2',
            isSignedIn: false,
            pic: 'chrome://theme/IDR_LOGIN_DEFAULT_USER_2',
          }
        ]);
      });
    }

    /** @override */
    addAccount() {
      // TODO(https://crbug.com/952232): Implement
      console.log('addKerberosAccount');
    }

    /** @override */
    reauthenticateAccount(principalName) {
      // TODO(https://crbug.com/952232): Implement
      console.log('reauthenticateKerberosAccount');
    }

    /** @override */
    removeAccount(account) {
      // TODO(https://crbug.com/952232): Implement
      console.log('removeKerberosAccount');
    }
  }

  cr.addSingletonGetter(KerberosAccountsBrowserProxyImpl);

  return {
    KerberosAccountsBrowserProxy: KerberosAccountsBrowserProxy,
    KerberosAccountsBrowserProxyImpl: KerberosAccountsBrowserProxyImpl,
  };
});
