// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Checks visibility of tab handles against expectations, and navigates to all
 * tabs with visible handles, validating visibility of all other tabs as it
 * goes.
 */
netInternalsTest.test('netInternalsTourTabs', function() {
  // Prevent sending any events to the browser as we flip through tabs, since
  // this tests is just intended to make sure everything's created and hooked
  // up properly Javascript side.
  g_browser.disable();

  // Expected visibility state of each tab.
  var tabVisibilityState = {
    capture: true,
    export: true,
    import: true,
    proxy: true,
    events: true,
    timeline: true,
    dns: true,
    sockets: true,
    spdy: true,
    httpCache: true,
    httpThrottling: true,
    serviceProviders: cr.isWindows,
    tests: true,
    hsts: true,
    logs: cr.isChromeOS,
    prerender: true,
    chromeos: cr.isChromeOS
  };

  netInternalsTest.checkTabHandleVisibility(tabVisibilityState, true);

  testDone();
});
