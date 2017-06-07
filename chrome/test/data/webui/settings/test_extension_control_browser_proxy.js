// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.ExtensionControlBrowserProxy}
 * @extends {TestBrowserProxy}
 */
function TestExtensionControlBrowserProxy() {
  TestBrowserProxy.call(this, [
    'disableExtension',
    'manageExtension',
  ]);
}

TestExtensionControlBrowserProxy.prototype = {
  __proto__: TestBrowserProxy.prototype,

  /** @override */
  disableExtension: function(extensionId) {
    this.methodCalled('disableExtension', extensionId);
  },

  /** @override */
  manageExtension: function(extensionId) {
    this.methodCalled('manageExtension', extensionId);
  },
};
