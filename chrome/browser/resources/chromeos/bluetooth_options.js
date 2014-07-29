// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of the BluetoothOptions calls from
   * BluetoothOptionsHandler that is registered by the webUI,
   * ie, BluetoothPairingUI.
   * @constructor
   */
  function BluetoothOptions() {
    OptionsPage.call(this,
                     'bluetooth',
                     '',
                     'bluetooth-container');
  }

  cr.addSingletonGetter(BluetoothOptions);

  BluetoothOptions.prototype = {
    __proto__: OptionsPage.prototype,
  };

  BluetoothOptions.updateDiscovery = function() {
  };

  // Export
  return {
    BluetoothOptions: BluetoothOptions
  };
});
