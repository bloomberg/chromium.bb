// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communcation for the system page. */

cr.define('settings', function() {
  /** @interface */
  function SystemPageBrowserProxy() {}

  SystemPageBrowserProxy.prototype = {
    /** Shows the native system proxy settings. */
    showProxySettings: function() {},

    /**
     * @return {boolean} Whether hardware acceleration was enabled when the user
     *     started Chrome.
     */
    wasHardwareAccelerationEnabledAtStartup: function() {},
  };

  /**
   * @constructor
   * @implements {settings.SystemPageBrowserProxy}
   */
  function SystemPageBrowserProxyImpl() {}

  cr.addSingletonGetter(SystemPageBrowserProxyImpl);

  SystemPageBrowserProxyImpl.prototype = {
    /** @override */
    showProxySettings: function() {
      chrome.send('showProxySettings');
    },

    /** @override */
    wasHardwareAccelerationEnabledAtStartup: function() {
      return loadTimeData.getBoolean('hardwareAccelerationEnabledAtStartup');
    },
  };

  return {
    SystemPageBrowserProxy: SystemPageBrowserProxy,
    SystemPageBrowserProxyImpl: SystemPageBrowserProxyImpl,
  };
});
