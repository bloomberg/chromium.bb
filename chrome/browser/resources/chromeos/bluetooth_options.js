// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;

  /**
   * Encapsulated handling of the BluetoothOptions calls from
   * BluetoothOptionsHandler that is registered by the webUI,
   * ie, BluetoothPairingUI.
   * @constructor
   */
  function BluetoothOptions() {
    Page.call(this, 'bluetooth', '', 'bluetooth-container');
  }

  cr.addSingletonGetter(BluetoothOptions);

  BluetoothOptions.prototype = {
    __proto__: Page.prototype,
  };

  BluetoothOptions.updateDiscovery = function() {
  };

  // Export
  return {
    BluetoothOptions: BluetoothOptions
  };
});
