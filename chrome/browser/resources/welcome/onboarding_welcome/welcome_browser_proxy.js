// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the welcome page to interact with
 * the browser.
 */

cr.define('welcome', function() {

  /** @interface */
  class WelcomeBrowserProxy {
    handleActivateSignIn() {}
    goToNewTabPage() {}
  }

  /** @implements {welcome.WelcomeBrowserProxy} */
  class WelcomeBrowserProxyImpl {
    /** @override */
    handleActivateSignIn() {
      // TODO(scottchen): add code to direct back to /welcome/returning-user
      //     after user finishes signing in.
      chrome.send('handleActivateSignIn');
    }

    /** @override */
    goToNewTabPage() {
      window.location.replace('chrome://newtab');
    }
  }

  cr.addSingletonGetter(WelcomeBrowserProxyImpl);

  return {
    WelcomeBrowserProxy: WelcomeBrowserProxy,
    WelcomeBrowserProxyImpl: WelcomeBrowserProxyImpl,
  };
});
