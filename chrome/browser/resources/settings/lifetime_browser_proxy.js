// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  class LifetimeBrowserProxy {
    // Triggers a browser restart.
    restart() {}

    // Triggers a browser relaunch.
    relaunch() {}

    // <if expr="chromeos">
    // First signs out current user and then performs a restart.
    signOutAndRestart() {}

    // Triggers a factory reset.
    factoryReset() {}
    // </if>
  }

  /**
   * @implements {settings.LifetimeBrowserProxy}
   */
  class LifetimeBrowserProxyImpl {
    /** @override */
    restart() {
      chrome.send('restart');
    }

    /** @override */
    relaunch() {
      chrome.send('relaunch');
    }

    // <if expr="chromeos">
    /** @override */
    signOutAndRestart() {
      chrome.send('signOutAndRestart');
    }

    /** @override */
    factoryReset() {
      chrome.send('factoryReset');
    }
    // </if>
  }

  cr.addSingletonGetter(LifetimeBrowserProxyImpl);

  return {
    LifetimeBrowserProxy: LifetimeBrowserProxy,
    LifetimeBrowserProxyImpl: LifetimeBrowserProxyImpl,
  };
});
