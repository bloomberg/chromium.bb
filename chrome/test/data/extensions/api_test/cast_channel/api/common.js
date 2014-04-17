// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertOpenChannel = function(channel) {
  chrome.test.assertTrue(!!channel);
  chrome.test.assertTrue(channel.channelId > 0);
  chrome.test.assertTrue(channel.url == 'cast://192.168.1.1:8009');
  chrome.test.assertTrue(channel.connectInfo.ipAddress == '192.168.1.1');
  chrome.test.assertTrue(channel.connectInfo.port == 8009);
  chrome.test.assertTrue(channel.connectInfo.auth == 'ssl');
  chrome.test.assertTrue(channel.readyState == 'open');
  chrome.test.assertTrue(channel.errorState == undefined);
};

assertClosedChannel = function(channel) {
  chrome.test.assertTrue(!!channel);
  chrome.test.assertTrue(channel.channelId > 0);
  chrome.test.assertTrue(channel.url == 'cast://192.168.1.1:8009');
  chrome.test.assertTrue(channel.connectInfo.ipAddress == '192.168.1.1');
  chrome.test.assertTrue(channel.connectInfo.port == 8009);
  chrome.test.assertTrue(channel.connectInfo.auth == 'ssl');
  chrome.test.assertTrue(channel.readyState == 'closed');
  chrome.test.assertTrue(channel.errorState == undefined);
};
