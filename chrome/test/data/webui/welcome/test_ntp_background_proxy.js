// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {NtpBackgroundProxy} */
class TestNtpBackgroundProxy extends TestBrowserProxy {
  constructor() {
    super([
      'clearBackground',
      'getBackgrounds',
      'preloadImage',
      'setBackground',
    ]);

    /** @private {!Array<!nux.NtpBackgroundData} */
    this.backgroundsList_ = [];
  }

  /** @override */
  clearBackground() {
    this.methodCalled('clearBackground');
  }

  /** @override */
  getBackgrounds() {
    this.methodCalled('getBackgrounds');
    return Promise.resolve(this.backgroundsList_);
  }

  /** @override */
  preloadImage(url) {
    this.methodCalled('preloadImage');
    return Promise.resolve();
  }

  /** @override */
  setBackground(id) {
    this.methodCalled('setBackground', id);
  }

  /** @param {!Array<!nux.NtpBackgroundData>} backgroundsList */
  setBackgroundsList(backgroundsList) {
    this.backgroundsList_ = backgroundsList;
  }
}
