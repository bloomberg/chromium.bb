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

  commandLineSwitches: [{switchName: 'enable-features',
                         switchValue: 'MaterialDesignHistory'}],

  /** @override */
  runAccessibilityChecks: false,

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'test_util.js',
    'browser_service_test.js',
    'history_drawer_test.js',
    'history_grouped_list_test.js',
    'history_item_test.js',
    'history_list_test.js',
    'history_metrics_test.js',
    'history_overflow_menu_test.js',
    'history_routing_test.js',
    'history_supervised_user_test.js',
    'history_synced_tabs_test.js',
    'history_toolbar_test.js',
  ]),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    suiteSetup(function() {
      // Wait for the top-level app element to be upgraded.
      return waitForAppUpgrade()
          .then(function() {
            // <iron-list>#_maxPages controls the default number of "pages" of
            // "physical" (i.e. DOM) elements to render. Some of these tests
            // rely on rendering up to 3 "pages" of items, which was previously
            // the default, changeed to 2 for performance reasons. TODO(dbeam):
            // maybe trim down the number of items created in the tests? Or
            // don't touch <iron-list>'s physical items as much?
            Array.from(document.querySelectorAll('* /deep/ iron-list')).forEach(
                function(ironList) { ironList._maxPages = 3; });
          })
          .then(function() { return md_history.ensureLazyLoaded(); })
          .then(function() {
            $('history-app').queryState_.queryingDisabled = true;
          });
    });
  },
};

TEST_F('MaterialHistoryBrowserTest', 'BrowserServiceTest', function() {
  md_history.browser_service_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'DrawerTest', function() {
  md_history.history_drawer_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'HistoryGroupedListTest', function() {
  md_history.history_grouped_list_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'HistoryItemTest', function() {
  md_history.history_item_test.registerTests();
  mocha.run();
});

// Flaky on chromeos, http://crbug.com/640862
TEST_F('MaterialHistoryBrowserTest', 'DISABLED_HistoryListTest', function() {
  md_history.history_list_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'Metrics', function() {
  md_history.history_metrics_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'HistoryToolbarTest', function() {
  md_history.history_toolbar_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'HistoryToolbarFocusTest', function() {
  md_history.history_toolbar_focus_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'HistoryOverflowMenuTest', function() {
  md_history.history_overflow_menu_test.registerTests();
  mocha.run();
});

TEST_F('MaterialHistoryBrowserTest', 'RoutingTest', function() {
  md_history.history_routing_test.registerTests();
  mocha.run();
});

// Fails on Mac, http://crbug.com/640862
TEST_F('MaterialHistoryBrowserTest', 'DISABLED_SyncedTabsTest', function() {
  md_history.history_synced_tabs_test.registerTests();
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

function MaterialHistoryWithQueryParamTest() {}

MaterialHistoryWithQueryParamTest.prototype = {
  __proto__: MaterialHistoryBrowserTest.prototype,

  browsePreload: 'chrome://history?q=query',

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // This message handler needs to be registered before the test since the
    // query can happen immediately after the element is upgraded. However,
    // since there may be a delay as well, the test might check the global var
    // too early as well. In this case the test will have overtaken the
    // callback.
    registerMessageCallback('queryHistory', this, function (info) {
      window.historyQueryInfo = info;
    });

    suiteSetup(function() {
      // Wait for the top-level app element to be upgraded.
      return waitForAppUpgrade().then(function() {
        md_history.ensureLazyLoaded();
      });
    });
  },
};

TEST_F('MaterialHistoryWithQueryParamTest', 'RoutingTestWithQueryParam',
  function() {
    md_history.history_routing_test_with_query_param.registerTests();
    mocha.run();
});
