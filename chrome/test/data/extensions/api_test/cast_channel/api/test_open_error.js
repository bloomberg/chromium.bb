// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var onClose = function(channel) {
  assertClosedChannel(channel);
}

var onError = function(channel) {
  chrome.cast.channel.close(channel, onClose);
  chrome.test.notifyPass();
}

var onOpen = function(channel) {
  assertClosedChannel(channel);
  chrome.test.succeed();
};

chrome.cast.channel.onError.addListener(onError);
chrome.cast.channel.open('cast://192.168.1.1:8009', onOpen);
