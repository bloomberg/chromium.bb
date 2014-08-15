// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var errorEvent = false;
var openCallback = false;

var onClose = function(channel) {
  assertClosedChannelWithError(channel, 'connect_error');
  chrome.test.succeed();
  chrome.test.notifyPass();
}

var onError = function(channel, error) {
  errorEvent = true;
  chrome.test.assertTrue(error.errorState == 'connect_error');
  chrome.test.assertTrue(error.challengeReplyErrorType == 9);
  chrome.test.assertTrue(error.nssErrorCode == -8164);
  chrome.test.assertTrue(error.netReturnValue == 0);
  maybeClose(channel);
}

var onOpen = function(channel) {
  openCallback = true;
  assertClosedChannelWithError(channel, 'connect_error');
  maybeClose(channel);
};

var maybeClose = function(channel) {
  if (errorEvent && openCallback) {
    console.log("closing " + JSON.toString(channel));
    chrome.cast.channel.close(channel, onClose);
  }
};

chrome.cast.channel.onError.addListener(onError);
chrome.cast.channel.open('cast://192.168.1.1:8009', onOpen);
