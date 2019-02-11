// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-app>', function() {
  let app;

  setup(function() {
    app = document.createElement('app-management-app');
    document.body.appendChild(app);
  });

  test('loads', async function() {
    // Check that the browser responds to the getApps() message.
    const {apps: initialApps} =
        await app_management.BrowserProxy.getInstance().handler.getApps();
  });

  test('Searching switches to search page', async function() {
    app.$$('cr-toolbar').fire('search-changed', 'SearchTest');
    assert(app.$$('app-management-search-view'));
  });
});
