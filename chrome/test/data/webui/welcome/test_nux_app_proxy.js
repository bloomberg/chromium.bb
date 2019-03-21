// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {nux.AppProxy} */
class TestNuxAppProxy extends TestBrowserProxy {
  constructor() {
    super([
      'cacheBookmarkIcon',
      'getAppList',
      'recordProviderSelected',
    ]);

    this.providerSelectedCount = 0;

    /** @private {!Array<!nux.BookmarkListItem>} */
    this.appList_ = [];
  }

  /** @override */
  getAppList() {
    this.methodCalled('getAppList');
    return Promise.resolve(this.appList_);
  }

  /** @override */
  cacheBookmarkIcon() {
    this.methodCalled('cacheBookmarkIcon');
  }

  /** @override */
  recordProviderSelected(providerId) {
    this.methodCalled('recordProviderSelected', providerId);
    this.providerSelectedCount++;
  }

  /** @param {!Array<!nux.BookmarkListItem>} appList */
  setAppList(appList) {
    this.appList_ = appList;
  }
}
