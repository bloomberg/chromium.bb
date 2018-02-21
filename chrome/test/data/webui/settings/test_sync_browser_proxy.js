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
      'startSignIn',
      'startSyncingWithEmail',
      'getPromoImpressionCount',
      'incrementPromoImpressionCount',
    ]);

    /** @private {number} */
    this.impressionCount_ = 0;
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

  /** @override */
  startSignIn() {
    this.methodCalled('startSignIn');
  }

  /** @override */
  startSyncingWithEmail(email) {
    this.methodCalled('startSyncingWithEmail', email);
  }

  setImpressionCount(count) {
    this.impressionCount_ = count;
  }

  /** @override */
  getPromoImpressionCount() {
    this.methodCalled('getPromoImpressionCount');
    return this.impressionCount_;
  }

  /** @override */
  incrementPromoImpressionCount() {
    this.methodCalled('incrementPromoImpressionCount');
  }
}