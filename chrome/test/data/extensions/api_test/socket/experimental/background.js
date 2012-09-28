// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const socket = chrome.socket;
var sid;

var onAccept = function(acceptInfo) {
  // The result is undefined because the manifest does not specify the experimental permission.
  chrome.test.assertEq(undefined, acceptInfo);
  chrome.test.succeed();
};

var onListen = function(result) {
  // The result is undefined because the manifest does not specify the experimental permission.
  chrome.test.assertEq(undefined, result);
  socket.accept(sid, onAccept);
};

var onCreate = function(socketInfo) {
  sid = socketInfo.socketId;
  socket.listen(sid, '127.0.0.1', 1234, onListen);
};

var onMessageReply = function() {
  socket.create('tcp', {}, onCreate);
};

chrome.test.sendMessage("ready", onMessageReply);
