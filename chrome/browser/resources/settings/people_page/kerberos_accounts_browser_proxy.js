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
      return cr.sendWithPromise('getKerberosAccounts');
    }

    /** @override */
    addAccount() {
      chrome.send('addKerberosAccount');
    }

    /** @override */
    reauthenticateAccount(principalName) {
      chrome.send('reauthenticateKerberosAccount', [principalName]);
    }

    /** @override */
    removeAccount(account) {
      chrome.send('removeKerberosAccount', [account.principalName]);
    }
  }

  cr.addSingletonGetter(KerberosAccountsBrowserProxyImpl);

  return {
    KerberosAccountsBrowserProxy: KerberosAccountsBrowserProxy,
    KerberosAccountsBrowserProxyImpl: KerberosAccountsBrowserProxyImpl,
  };
});
