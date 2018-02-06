// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.SyncBrowserProxy} */
class TestSyncBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getSyncStatus',
      'getStoredAccounts',
      'signOut',
    ]);
  }

  /** @override */
  getSyncStatus() {
    this.methodCalled('getSyncStatus');
    return Promise.resolve({
      signedIn: true,
      signedInUsername: 'fakeUsername'
    });
  }

  /** @override */
  getStoredAccounts() {
    this.methodCalled('getStoredAccounts');
    return Promise.resolve([]);
  }

  /** @override */
  signOut(deleteProfile) {
    this.methodCalled('signOut', deleteProfile);
  }
}