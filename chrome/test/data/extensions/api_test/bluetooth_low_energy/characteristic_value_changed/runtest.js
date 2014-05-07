// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testCharacteristicValueChanged() {
  chrome.test.assertEq(2, Object.keys(changedChrcs).length);

  chrome.test.assertEq(charId0, changedChrcs[charId0].instanceId);
  chrome.test.assertEq(charId2, changedChrcs[charId2].instanceId);

  chrome.test.succeed();
}

var charId0 = 'char_id0';
var charId2 = 'char_id2';

var changedChrcs = {}

chrome.bluetoothLowEnergy.onCharacteristicValueChanged.addListener(
    function (chrc) {
      changedChrcs[chrc.instanceId] = chrc;
});

chrome.test.sendMessage('ready', function (message) {
  chrome.test.runTests([testCharacteristicValueChanged]);
});
