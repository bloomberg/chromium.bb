// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<app-management-app>', function() {
  test('loads', function(done) {
    app_management.BrowserProxy.getInstance().handler.getApps();

    let callbackRouter =
        app_management.BrowserProxy.getInstance().callbackRouter;
    callbackRouter.onAppsAdded.addListener(() => done());
  });
});
