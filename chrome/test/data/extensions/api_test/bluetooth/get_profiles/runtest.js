// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetProfiles() {
  chrome.test.assertEq(2, profiles.length);
  chrome.test.assertEq('1234', profiles[0].uuid);
  chrome.test.assertEq('5678', profiles[1].uuid);
  chrome.test.succeed();
}

var profiles = [];

function failOnError() {
  if (chrome.runtime.lastError) {
    chrome.test.fail(chrome.runtime.lastError.message);
  }
}

chrome.bluetooth.getProfiles(
  {
    device: {
    name: 'device',
    address: '11:12:13:14:15:16',
    paired: false,
    connected: false
    }
  },
  function(results) {
    failOnError();
    profiles = results;
    chrome.test.sendMessage('ready',
      function(message) {
        chrome.test.runTests([testGetProfiles]);
      });
  });
