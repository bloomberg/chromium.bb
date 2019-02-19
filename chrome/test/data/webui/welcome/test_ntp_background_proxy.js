// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {NtpBackgroundProxy} */
class TestNtpBackgroundProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getBackgrounds',
      'setBackground',
    ]);

    /** @private {!Array<!nux.NtpBackgroundData} */
    this.backgroundsList_ = [];
  }

  /** @override */
  getBackgrounds() {
    this.methodCalled('getBackgrounds');
    return Promise.resolve(this.backgroundsList_);
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
