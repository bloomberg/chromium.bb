// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function onMessage() {
      chrome.test.listenOnce(chrome.gcm.onMessage, function(message) {
        chrome.test.assertEq(2, Object.keys(message.data).length);
        chrome.test.assertTrue(message.data.hasOwnProperty('property1'));
        chrome.test.assertTrue(message.data.hasOwnProperty('property2'));
        chrome.test.assertEq('value1', message.data.property1);
        chrome.test.assertEq('value2', message.data.property2);
      });
    }
  ]);
};
