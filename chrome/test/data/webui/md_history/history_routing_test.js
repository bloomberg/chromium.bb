// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_routing_test', function() {
  function registerTests() {
    suite('routing-test', function() {
      var app;
      var list;

      suiteSetup(function() {
        app = $('history-app');
        sidebar = app.$['side-bar']
      });

      test('changing route changes active view', function() {
        assertEquals('history', app.$.content.selected);
        app.set('routeData_.page', 'syncedTabs');
        return flush().then(function() {
          assertEquals('syncedTabs', app.$.content.selected);
          assertEquals('chrome://history/syncedTabs', window.location.href);
        });
      });

      test('route updates from sidebar', function() {
        var menu = sidebar.$.menu;
        assertEquals('', app.routeData_.page);
        assertEquals('chrome://history/', window.location.href);

        MockInteractions.tap(menu.children[1]);
        assertEquals('syncedTabs', app.routeData_.page);
        assertEquals('chrome://history/syncedTabs', window.location.href);

        MockInteractions.tap(menu.children[0]);
        assertEquals('', app.routeData_.page);
        assertEquals('chrome://history/', window.location.href);
      });

      teardown(function() {
        app.set('routeData_.page', '');
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
