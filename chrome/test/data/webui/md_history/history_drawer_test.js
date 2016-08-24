// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_drawer_test', function () {
  function registerTests() {
    suite('drawer-test', function() {
      var app;

      suiteSetup(function() {
        app = $('history-app');
      });

      test('drawer has correct selection', function() {
        app.selectedPage_ = 'syncedTabs';
        app.hasDrawer_ = true;
        return flush().then(function() {
          var drawer = app.$$('#drawer');
          var drawerSideBar = app.$$('#drawer-side-bar');

          assertTrue(!!drawer);
          assertTrue(!!drawerSideBar);

          var menuButton = app.$.toolbar.$['main-toolbar'].$$('#menuButton');
          assertTrue(!!menuButton);

          MockInteractions.tap(menuButton);
          assertTrue(drawer.opened);

          assertEquals('syncedTabs', drawerSideBar.$.menu.selected);
        });
      });
    });
  }

  return {
    registerTests: registerTests
  };
});
