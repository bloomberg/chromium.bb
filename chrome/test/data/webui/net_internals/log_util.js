// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

/**
 * Exports a log dump to a string and loads it.  Makes sure no errors occur,
 * and checks visibility of tabs aftwards.  Does not actually save the log to a
 * file.
 * TODO(mmenke):  Add some checks for the import view.
 * TODO(mmenke):  Add a test for a log created with --log-net-log.
 */
TEST_F('NetInternalsTest', 'netInternalsExportImportDump', function() {
  expectFalse(g_browser.isDisabled());

  // Callback passed to |createLogDumpAsync|.  Tries to load the dumped log
  // file, and then checks tab visibility afterwards.
  // @param {string} logDumpText Log dump, as a string.
  function onLogDumpCreated(logDumpText) {
    expectEquals('Log loaded.', logutil.loadLogFile(logDumpText));

    expectTrue(g_browser.isDisabled());
    NetInternalsTest.expectStatusViewNodeVisible(StatusView.FOR_FILE_ID);

    var tabVisibilityState = {
      capture: false,
      export: false,
      import: true,
      proxy: true,
      events: true,
      timeline: true,
      dns: true,
      sockets: true,
      spdy: true,
      httpPipeline: false,
      httpCache: true,
      httpThrottling: false,
      serviceProviders: cr.isWindows,
      tests: false,
      hsts: false,
      logs: false,
      prerender: true,
      chromeos: false
    };

    NetInternalsTest.checkTabHandleVisibility(tabVisibilityState, false);
    testDone();
  }

  logutil.createLogDumpAsync('Log dump test', onLogDumpCreated);
});

})();  // Anonymous namespace
