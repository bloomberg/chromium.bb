// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('app state', function() {
  let apps;
  let action;

  function createApp(id, config) {
    return app_management.FakePageHandler.createApp(id, config);
  }

  setup(function() {
    // Create an initial AppMap.
    apps = {
      '1': createApp('1'),
    };
  });

  test('updates when apps are added', function() {
    appsToAdd = [
      createApp('2', {type: 0, title: 'b'}),
      createApp('3'),
    ];

    action = app_management.actions.addApps(appsToAdd);
    apps = app_management.AppState.updateApps(apps, action);

    // Check that apps contains a key for each app id.
    assertTrue(!!apps['1']);
    assertTrue(!!apps['2']);
    assertTrue(!!apps['3']);

    // Check that id corresponds to the right app.
    let app = apps['2'];
    assertEquals('2', app.id);
    assertEquals(0, app.type);
    assertEquals('b', app.title);
  });
});
