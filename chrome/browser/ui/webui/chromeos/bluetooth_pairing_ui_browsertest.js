// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for Bluetooth pairing dialog WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function BluetoothPairingUITest() {
}

BluetoothPairingUITest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  typedefCppFixture: 'BluetoothPairingUITest',

  /** @override */
  testGenCppIncludes: function() {
    GEN('#include "chrome/browser/ui/webui/chromeos/' +
        'bluetooth_pairing_ui_browsertest-inl.h"');
  },

  /** @override */
  testGenPreamble: function() {
    GEN('ShowDialog();');
  },
};

TEST_F('BluetoothPairingUITest', 'Basic', function() {
  assertEquals('chrome://bluetooth-pairing/', document.location.href);
});
