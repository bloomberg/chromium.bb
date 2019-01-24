// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  class BrowserProxy {
    constructor() {
      /** @type {mdDownloads.mojom.PageCallbackRouter} */
      this.callbackRouter = new mdDownloads.mojom.PageCallbackRouter();

      /** @type {mdDownloads.mojom.PageHandlerProxy} */
      this.handler = new mdDownloads.mojom.PageHandlerProxy();

      const factory = mdDownloads.mojom.PageHandlerFactory.getProxy();
      factory.createPageHandler(
          this.callbackRouter.createProxy(), this.handler.createRequest());
    }
  }

  cr.addSingletonGetter(BrowserProxy);

  return {BrowserProxy: BrowserProxy};
});
