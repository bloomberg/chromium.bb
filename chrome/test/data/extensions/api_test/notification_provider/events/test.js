// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var testEvents = function() {

  chrome.notificationProvider.onCreated.addListener(function(senderId,
                                                             notificationId,
                                                             options) {
    chrome.test.succeed();
  });

  chrome.notificationProvider.onUpdated.addListener(function(senderId,
                                                             notificationId,
                                                             options) {
    chrome.test.succeed();
  });

  chrome.notificationProvider.onCleared.addListener(function(senderId,
                                                             notificationId) {
    chrome.test.succeed();
  });
};

chrome.test.runTests([ testEvents ]);