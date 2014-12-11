// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for WebRTCDeviceProvider.
 * @constructor
 * @extends {testing.Test}
 */
function WebRTCDeviceProviderBrowserTest() {}

WebRTCDeviceProviderBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the worker.
   */
  browsePreload: 'chrome://webrtc-device-provider/background_worker.html',

  isAsync: true,
};

TEST_F('WebRTCDeviceProviderBrowserTest', 'TestLoads', function() {
  testDone();
});
