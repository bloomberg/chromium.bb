// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('signin_sync_confirmation', function() {

  suite('SigninSyncConfirmationTest', function() {
    let app;
    setup(function() {
      PolymerTest.clearBody();
      app = document.createElement('sync-confirmation-app');
      document.body.append(app);
    });

    // Tests that no DCHECKS are thrown during initialization of the UI.
    test('LoadPage', function() {
      assertEquals('Get even more from Chrome', app.$.heading.textContent);
    });
  });
});
