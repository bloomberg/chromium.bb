// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Exports a log dump to a string and loads it.  Makes sure no errors occur,
 * and checks visibility of tabs aftwards.
 */
netInternalsTest.test('netInternalsExportImportDump', function() {
  // Callback passed to |createLogDumpAsync|.  Tries to load the dumped log
  // file, and then checks tab visibility afterwards.
  // @param {string} logDumpText Log dump, as a string.
  function onLogDumpCreated(logDumpText) {
    expectEquals('Log loaded.', logutil.loadLogFile(logDumpText));

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
      httpCache: true,
      httpThrottling: false,
      serviceProviders: cr.isWindows,
      tests: false,
      hsts: false,
      logs: false,
      prerender: true
    };

    netInternalsTest.checkTabHandleVisibility(tabVisibilityState, false);
    testDone();
  }

  logutil.createLogDumpAsync('Log dump test', onLogDumpCreated);
});
