// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * A test version of LifetimeBrowserProxy.
   *
   * @constructor
   * @implements {settings.LifetimeBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestLifetimeBrowserProxy = function() {
    var methodNames = ['restart', 'relaunch'];
    if (cr.isChromeOS)
      methodNames.push('signOutAndRestart', 'factoryReset');

    settings.TestBrowserProxy.call(this, methodNames);
  };

  TestLifetimeBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    restart: function() {
      this.methodCalled('restart');
    },

    /** @override */
    relaunch: function() {
      this.methodCalled('relaunch');
    },
  };

  if (cr.isChromeOS) {
    /** @override */
    TestLifetimeBrowserProxy.prototype.signOutAndRestart = function() {
      this.methodCalled('signOutAndRestart');
    };

    /** @override */
    TestLifetimeBrowserProxy.prototype.factoryReset = function() {
      this.methodCalled('factoryReset');
    };
  }

  return {
    TestLifetimeBrowserProxy: TestLifetimeBrowserProxy,
  };
});
