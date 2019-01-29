// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @return {app_management.FakePageHandler}
 */
function setupFakeHandler() {
  const browserProxy = app_management.BrowserProxy.getInstance();
  const callbackRouterProxy = browserProxy.callbackRouter.createProxy();

  const fakeHandler = new app_management.FakePageHandler(callbackRouterProxy);
  browserProxy.handler = fakeHandler;

  return fakeHandler;
}

/**
 * Replace the app management store instance with a new, empty store.
 */
function replaceStore() {
  app_management.Store.instance_ = new app_management.Store();

  app_management.Store.getInstance().init(
      app_management.util.createEmptyState());
}

/**
 * @param {Element} element
 * @return {bool}
 */
function isHidden(element) {
  const rect = element.getBoundingClientRect();
  return rect.height === 0 && rect.width === 0;
}
