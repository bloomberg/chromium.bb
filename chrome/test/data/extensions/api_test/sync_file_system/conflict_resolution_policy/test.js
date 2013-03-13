// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testStep = [
  function testManualPolicy() {
    testConflictResolutionPolicy('manual', testStep.shift());
  },
  function testLastWriteWinPolicy() {
    testConflictResolutionPolicy('last_write_win', chrome.test.callbackPass());
  },
];

function testConflictResolutionPolicy(policy, callback) {
  var steps = [
    function setManualPolicy() {
      chrome.syncFileSystem.setConflictResolutionPolicy(
          policy, chrome.test.callbackPass(steps.shift()));
    },
    function getManualPolicy() {
      chrome.syncFileSystem.getConflictResolutionPolicy(
          chrome.test.callbackPass(steps.shift()));
    },
    function checkManualPolicy(policy_returned) {
      chrome.test.assertEq(policy, policy_returned);
      callback();
    }
  ];
  steps.shift()();
}

chrome.test.runTests([
  testStep.shift()
]);
