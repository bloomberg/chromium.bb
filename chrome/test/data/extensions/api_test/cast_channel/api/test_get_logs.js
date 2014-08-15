// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var message = {
  'namespace_': 'foo',
  'sourceId': 'src',
  'destinationId': 'dest',
  'data': 'some-string'
};

var onGetLogs = function(blob) {
  chrome.test.assertTrue(blob.byteLength > 0);
  chrome.test.succeed();
}

var onClose = function(channel) {
  assertClosedChannel(channel);
  chrome.cast.channel.getLogs(onGetLogs);
};

var onSend = function(channel) {
  assertOpenChannel(channel);
  chrome.cast.channel.close(channel, onClose);
};

var onOpen = function(channel) {
  assertOpenChannel(channel);
  chrome.cast.channel.send(channel, message, onSend);
};

chrome.cast.channel.open({ipAddress: '192.168.1.1', port: 8009, auth: 'ssl'},
                         onOpen);
