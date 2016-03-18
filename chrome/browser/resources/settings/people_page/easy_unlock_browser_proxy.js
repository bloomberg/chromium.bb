// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the the People section to interact
 * with the Easy Unlock functionality of the browser. ChromeOS only.
 */
cr.exportPath('settings');

cr.define('settings', function() {
  /** @interface */
  function EasyUnlockBrowserProxy() {}

  EasyUnlockBrowserProxy.prototype = {
    /**
     * Returns a true promise if Easy Unlock is already enabled on the device.
     * @return {!Promise<boolean>}
     */
    getEnabledStatus: function() {},

    /**
     * Starts the Easy Unlock setup flow.
     */
    launchSetup: function() {}
  };

  /**
   * @constructor
   * @implements {EasyUnlockBrowserProxy}
   */
  function EasyUnlockBrowserProxyImpl() {}
  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(EasyUnlockBrowserProxyImpl);

  EasyUnlockBrowserProxyImpl.prototype = {
    /** @override */
    getEnabledStatus: function() {
      return cr.sendWithPromise('easyUnlockGetEnabledStatus');
    },

    /** @override */
    launchSetup: function() {
      chrome.send('easyUnlockLaunchSetup');
    },
  };

  return {
    EasyUnlockBrowserProxyImpl: EasyUnlockBrowserProxyImpl,
  };
});
