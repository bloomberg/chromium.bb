// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('app_management', function() {
  /** @implements {appManagement.mojom.PageHandlerInterface} */
  class FakePageHandler {
    constructor(page) {
      /** @type {appManagement.mojom.PageInterface} */
      this.page = page;
    }

    getApps() {
      const fakeAppIdList = [
        'ahfgeienlihckogmohjhadlkjgocpleb',
        'aohghmighlieiainnegkcijnfilokake',
        'blpcfgokakmgnkcojhhkbfbldkacnbeo',
        'pjkljhegncpnkpknbcohdijeoejaedia',
        'aapocclcgogkmnckokdopfmhonfmgoek',
        'kbfnbcaeplbcioakkpcpgfkobkghlhen',
        'gmbgaklkmjakoegficnlkhebmhkjfich',
      ];

      this.page.onAppsAdded(fakeAppIdList);
    }
  }

  return {FakePageHandler: FakePageHandler};
});
