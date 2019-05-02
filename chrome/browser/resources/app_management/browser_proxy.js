// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('app_management', function() {
  class BrowserProxy {
    constructor() {
      /** @type {appManagement.mojom.PageCallbackRouter} */
      this.callbackRouter = new appManagement.mojom.PageCallbackRouter();

      /** @type {appManagement.mojom.PageHandlerInterface} */
      this.handler = null;

      const urlParams = new URLSearchParams(window.location.search);
      const useFake = urlParams.get('fakeBackend');

      if (useFake) {
        this.handler = new app_management.FakePageHandler(
            this.callbackRouter.createProxy());

        const /** @type {!Array<App>}*/ appList = [
          app_management.FakePageHandler.createApp(
              'ahfgeienlihckogmohjhadlkjgocpleb', {
                title: 'Web Store (system app)',
                installSource: InstallSource.kSystem
              }),
          app_management.FakePageHandler.createApp(
              'aohghmighlieiainnegkcijnfilokake',
              {title: 'Docs', type: AppType.kArc}),
          app_management.FakePageHandler.createApp(
              'blpcfgokakmgnkcojhhkbfbldkacnbeo',
              {title: 'Youtube', type: AppType.kCrostini}),
          app_management.FakePageHandler.createApp(
              'pjkljhegncpnkpknbcohdijeoejaedia', {title: 'Gmail'}),
          app_management.FakePageHandler.createApp(
              'aapocclcgogkmnckokdopfmhonfmgoek', {
                title: 'Slides (policy installed)',
                installSource: InstallSource.kPolicy
              }),
        ];

        this.handler.setApps(appList);

      } else {
        this.handler = new appManagement.mojom.PageHandlerProxy();
        const factory = appManagement.mojom.PageHandlerFactory.getProxy();
        factory.createPageHandler(
            this.callbackRouter.createProxy(), this.handler.$.createRequest());
      }
    }
  }

  cr.addSingletonGetter(BrowserProxy);

  return {BrowserProxy: BrowserProxy};
});
