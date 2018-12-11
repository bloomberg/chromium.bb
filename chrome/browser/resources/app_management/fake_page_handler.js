// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('app_management', function() {
  /** @implements {appManagement.mojom.PageHandlerInterface} */
  class FakePageHandler {
    constructor(page) {
      /** @type {appManagement.mojom.PageInterface} */
      this.page = page;

      /** @type {!Array<appManagement.mojom.App>} */
      this.apps_ = [
        this.createApp('ahfgeienlihckogmohjhadlkjgocpleb'),
        this.createApp(
            'aohghmighlieiainnegkcijnfilokake',
            {type: apps.mojom.AppType.kArc}),
        this.createApp('blpcfgokakmgnkcojhhkbfbldkacnbeo'),
        this.createApp('pjkljhegncpnkpknbcohdijeoejaedia'),
        this.createApp('aapocclcgogkmnckokdopfmhonfmgoek'),
      ];
    }

    getApps() {
      this.page.onAppsAdded(this.apps_);
    }

    /**
     * @param {string} id
     * @param {Object=} config
     * @return {!appManagement.mojom.App}
     */
    createApp(id, config) {
      const app = /** @type {!appManagement.mojom.App} */ ({
        id: id,
        type: apps.mojom.AppType.kUnknown,
        title: 'App Title',
      });
      if (config)
        Object.assign(app, config);

      return app;
    }
  }

  return {FakePageHandler: FakePageHandler};
});
