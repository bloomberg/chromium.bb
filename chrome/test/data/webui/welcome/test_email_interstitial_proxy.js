// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {nux.EmailInterstitialProxy} */
class TestEmailInterstititalProxy extends TestBrowserProxy {
  constructor() {
    super([
      'recordPageShown',
      'recordNavigatedAway',
      'recordSkip',
      'recordNext',
    ]);
  }

  /** @override */
  recordPageShown() {
    this.methodCalled('recordPageShown');
  }

  /** @override */
  recordNavigatedAway() {
    this.methodCalled('recordNavigatedAway');
  }

  /** @override */
  recordSkip() {
    this.methodCalled('recordSkip');
  }

  /** @override */
  recordNext() {
    this.methodCalled('recordNext');
  }
}
