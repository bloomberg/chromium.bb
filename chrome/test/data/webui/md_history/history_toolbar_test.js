// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('history-toolbar', function() {
  var app;
  var element;
  var toolbar;
  var TEST_HISTORY_RESULTS;

  suiteSetup(function() {
    TEST_HISTORY_RESULTS =
        [createHistoryEntry('2016-03-15', 'https://google.com')];
  });

  setup(function() {
    app = replaceApp();
    element = app.$.history;
    toolbar = app.$.toolbar;
    return PolymerTest.flushTasks();
  });

  test('selecting checkbox causes toolbar to change', function() {
    element.addNewResults(TEST_HISTORY_RESULTS);

    return PolymerTest.flushTasks().then(function() {
      var item = element.$$('history-item');
      MockInteractions.tap(item.$.checkbox);

      // Ensure that when an item is selected that the count held by the
      // toolbar increases.
      assertEquals(1, toolbar.count);
      // Ensure that the toolbar boolean states that at least one item is
      // selected.
      assertTrue(toolbar.itemsSelected_);

      MockInteractions.tap(item.$.checkbox);

      // Ensure that when an item is deselected the count held by the
      // toolbar decreases.
      assertEquals(0, toolbar.count);
      // Ensure that the toolbar boolean states that no items are selected.
      assertFalse(toolbar.itemsSelected_);
    });
  });

  test('search term gathered correctly from toolbar', function(done) {
    app.queryState_.queryingDisabled = false;
    registerMessageCallback('queryHistory', this, function(info) {
      assertEquals('Test', info[0]);
      app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
      done();
    });

    toolbar.$$('cr-toolbar').fire('search-changed', 'Test');
  });

  test('spinner is active on search' , function(done) {
    app.queryState_.queryingDisabled = false;
    registerMessageCallback('queryHistory', this, function(info) {
      PolymerTest.flushTasks().then(function() {
        assertTrue(toolbar.spinnerActive);
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        assertFalse(toolbar.spinnerActive);
        done();
      });
    });

    toolbar.$$('cr-toolbar').fire('search-changed', 'Test2');
  });

  test('sync notice shows and hides', function() {
    toolbar.showSyncNotice = true;
    Polymer.dom.flush();

    var button = toolbar.$$('#info-button');
    var notice = toolbar.$$('#sync-notice');

    assertTrue(!!button);
    MockInteractions.tap(button);

    assertFalse(notice.hidden);

    // Tapping the notice does not dismiss it.
    MockInteractions.tap(notice);
    assertFalse(notice.hidden);

    // Tapping elsewhere does dismiss the notice.
    MockInteractions.tap(toolbar);
    assertTrue(notice.hidden);

    // Pressing escape hides the notice.
    MockInteractions.tap(button);
    assertFalse(notice.hidden);
    MockInteractions.pressAndReleaseKeyOn(notice, /* esc */ 27, '', 'Escape');
    assertTrue(notice.hidden);

    // Hiding the button hides the notice.
    MockInteractions.tap(button);
    toolbar.showSyncNotice = false;
    assertTrue(notice.hidden);
  });

  test('menu promo hides when drawer is opened', function() {
    app.showMenuPromo_ = true;
    app.hasDrawer_ = true;
    Polymer.dom.flush();
    MockInteractions.tap(toolbar.$['main-toolbar'].$$('#menuButton'));
    assertFalse(app.showMenuPromo_);
  });

  teardown(function() {
    registerMessageCallback('queryHistory', this, function() {});
  });
});
