// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {nux.NuxEmailProxy} */
class TestNuxEmailProxy extends TestBrowserProxy {
  constructor() {
    super([
      'cacheBookmarkIcon',
      'getEmailList',
      'getSavedProvider',
      'recordPageInitialized',
      'recordClickedOption',
      'recordClickedDisabledButton',
      'recordProviderSelected',
      'recordNoThanks',
      'recordGetStarted',
      'recordFinalize',
    ]);

    /** @private {!Array<!nux.BookmarkListItem>} */
    this.emailList_ = [];

    /** @private {number} */
    this.stubSavedProvider_;
  }

  /** @param {number} id */
  setSavedProvider(id) {
    this.stubSavedProvider_ = id;
  }

  /** @override */
  recordPageInitialized() {
    this.methodCalled('recordPageInitialized');
  }

  /** @override */
  getEmailList() {
    this.methodCalled('getEmailList');
    return Promise.resolve(this.emailList_);
  }

  /** @override */
  cacheBookmarkIcon() {
    this.methodCalled('cacheBookmarkIcon');
  }
  /** @override */
  getSavedProvider() {
    this.methodCalled('getSavedProvider');
    return this.stubSavedProvider_;
  }
  /** @override */
  recordClickedOption() {
    this.methodCalled('recordClickedOption');
  }
  /** @override */
  recordClickedDisabledButton() {
    this.methodCalled('recordClickedDisabledButton');
  }
  /** @override */
  recordProviderSelected() {
    this.methodCalled('recordProviderSelected', arguments);
  }
  /** @override */
  recordNoThanks() {
    this.methodCalled('recordNoThanks');
  }
  /** @override */
  recordGetStarted() {
    this.methodCalled('recordGetStarted');
  }
  /** @override */
  recordFinalize() {
    this.methodCalled('recordFinalize');
  }

  /** @param {!Array<!nux.BookmarkListItem>} emailList */
  setEmailList(emailList) {
    this.emailList_ = emailList;
  }
}
