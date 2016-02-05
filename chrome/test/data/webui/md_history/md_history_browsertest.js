// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Material Design history page.
 */

var ROOT_PATH = '../../../../../';

GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "base/command_line.h"');
GEN('#include "chrome/test/data/webui/history_ui_browsertest.h"');

function MaterialHistoryBrowserTest() {}

MaterialHistoryBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://history',

  commandLineSwitches: [{switchName: 'enable-md-history'}],

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'test_util.js',
    'history_card_manager_test.js',
    'history_card_test.js',
    'history_overflow_menu_test.js',
    'history_supervised_user_test.js',
    'history_toolbar_test.js'
  ])
};

TEST_F('MaterialHistoryBrowserTest', 'HistoryCardTest', function() {
  md_history.history_card_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'HistoryCardManagerTest', function() {
  md_history.history_card_manager_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'HistoryToolbarTest', function() {
  md_history.history_toolbar_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'HistoryOverflowMenuTest', function() {
  md_history.history_overflow_menu_test.registerTests();
  mocha.run();
});

function MaterialHistoryDeletionDisabledTest() {}

MaterialHistoryDeletionDisabledTest.prototype = {
  __proto__: MaterialHistoryBrowserTest.prototype,

  typedefCppFixture: 'HistoryUIBrowserTest',

  testGenPreamble: function() {
    GEN('  SetDeleteAllowed(false);');
  }
};

TEST_F('MaterialHistoryDeletionDisabledTest', 'HistorySupervisedUserTest',
    function() {
  md_history.history_supervised_user_test.registerTests();
  mocha.run();
});
