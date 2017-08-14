// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.ProfileInfoBrowserProxy} */
class TestProfileInfoBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getProfileInfo',
      'getProfileStatsCount',
      'getProfileManagesSupervisedUsers',
    ]);

    this.fakeProfileInfo = {
      name: 'fakeName',
      iconUrl: 'http://fake-icon-url.com/',
    };
  }

  /** @override */
  getProfileInfo() {
    this.methodCalled('getProfileInfo');
    return Promise.resolve(this.fakeProfileInfo);
  }

  /** @override */
  getProfileStatsCount() {
    this.methodCalled('getProfileStatsCount');
  }

  /** @override */
  getProfileManagesSupervisedUsers() {
    this.methodCalled('getProfileManagesSupervisedUsers');
    return Promise.resolve(false);
  }
}