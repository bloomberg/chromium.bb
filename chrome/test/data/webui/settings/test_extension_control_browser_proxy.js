// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.ExtensionControlBrowserProxy}
 * @extends {settings.TestBrowserProxy}
 */
function TestExtensionControlBrowserProxy() {
  settings.TestBrowserProxy.call(this, [
    'disableExtension',
    'manageExtension',
  ]);
}

TestExtensionControlBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /** @override */
  disableExtension: function(extensionId) {
    this.methodCalled('disableExtension', extensionId);
  },

  /** @override */
  manageExtension: function(extensionId) {
    this.methodCalled('manageExtension', extensionId);
  },
};
