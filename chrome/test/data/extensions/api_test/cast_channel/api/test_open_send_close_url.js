// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var message = {
  'namespace_': 'foo',
  'sourceId': 'src',
  'destinationId': 'dest',
  'data': 'some-string'
};

var onClose = function(channel) {
  assertClosedChannel(channel);
  chrome.test.succeed();
};

var onSend = function(channel) {
  assertOpenChannel(channel);
  chrome.cast.channel.close(channel, onClose);
};

var onOpen = function(channel) {
  assertOpenChannel(channel);
  chrome.cast.channel.send(channel, message, onSend);
};

chrome.cast.channel.open('cast://192.168.1.1:8009', onOpen);
