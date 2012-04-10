// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const socket = chrome.experimental.socket;
const message = "helloECHO";
const address = "127.0.0.1";
var protocol = "none";
var port = -1;
var socketId = 0;
var dataRead = "";
var succeeded = false;
var waitCount = 0;

var testSocketCreation = function() {
  function onCreate(socketInfo) {
    chrome.test.assertTrue(socketInfo.socketId > 0);

    // TODO(miket): this doesn't work yet. It's possible this will become
    // automatic, but either way we can't forget to clean up.
    //socket.destroy(socketInfo.socketId);

    chrome.test.succeed();
  }

  socket.create(protocol, address, port, {}, onCreate);
};

// net/tools/testserver/testserver.py is picky about the format of what
// it calls its "echo" messages. One might go so far as to mutter to
// oneself that it isn't an echo server at all.
//
// The response is based on the request but obfuscated using a random
// key.
const request = "0100000005320000005hello";
var expectedResponsePattern = /0100000005320000005.{11}/;

var address;
var protocol;
var port;
var socketId = 0;
var bytesWritten = 0;
var dataRead = "";
var succeeded = false;
var waitCount = 0;

var testSocketCreation = function() {
  function onCreate(socketInfo) {
    chrome.test.assertTrue(socketInfo.socketId > 0);

    // TODO(miket): this doesn't work yet. It's possible this will become
    // automatic, but either way we can't forget to clean up.
    //socket.destroy(socketInfo.socketId);

    chrome.test.succeed();
  }

  socket.create(protocol, address, port, {onEvent: function(e) {}}, onCreate);
};

function onDataRead(readInfo) {
  dataRead += readInfo.message;
  if (dataRead.match(expectedResponsePattern)) {
    succeeded = true;
    chrome.test.succeed();
  }
  // Blocked. Wait for onEvent.
}

function onWriteComplete(writeInfo) {
  bytesWritten += writeInfo.bytesWritten;
  if (bytesWritten == request.length) {
    socket.read(socketId, onDataRead);
  }
  // Blocked. Wait for onEvent.
}

function onConnectComplete(connectResult) {
  if (connectResult == 0) {
    socket.write(socketId, request, onWriteComplete);
  }
  // Blocked. Wait for onEvent.
}

function onCreate(socketInfo) {
  socketId = socketInfo.socketId;
  chrome.test.assertTrue(socketId > 0, "failed to create socket");
  socket.connect(socketId, onConnectComplete);
}

function onEvent(socketEvent) {
  if (socketEvent.type == "connectComplete") {
    onConnectComplete(socketEvent.resultCode);
  } else if (socketEvent.type == "dataRead") {
    // TODO(miket): why one "message" and the other "data"?
    onDataRead({message: socketEvent.data});
  } else if (socketEvent.type == "writeComplete") {
    onWriteComplete(socketEvent.resultCode);
  } else {
    console.log("Received unhandled socketEvent of type " + socketEvent.type);
  }
};

function onRead(readInfo) {
  if (readInfo.message == message) {
    succeeded = true;
    chrome.test.succeed();
  } else {
    // The read blocked. Save what we've got so far, and wait for onEvent.
    dataRead = readInfo.message;
  }
}

function onWrite(writeInfo) {
  chrome.test.assertTrue(writeInfo.bytesWritten == message.length);
  socket.read(socketId, onRead);
}

function onConnect(connectResult) {
  chrome.test.assertTrue(connectResult);
  socket.write(socketId, message, onWrite);
}

function waitForBlockingOperation() {
  if (++waitCount < 10) {
    setTimeout(waitForBlockingOperation, 1000);
  } else {
    // We weren't able to succeed in the given time.
    chrome.test.fail("Operations didn't complete after " + waitCount + " " +
                     "seconds. Response so far was <" + dataRead + ">.");
  }
}

var testSending = function() {
  dataRead = "";
  succeeded = false;
  waitCount = 0;

  setTimeout(waitForBlockingOperation, 1000);
  socket.create(protocol, address, port, { onEvent: onEvent }, onCreate);
};

var onMessageReply = function(message) {
  var parts = message.split(":");
  protocol = parts[0];
  address = parts[1];
  port = parseInt(parts[2]);
  console.log("Running tests, protocol " + protocol + ", echo server " +
              address + ":" + port);
  chrome.test.runTests([ testSocketCreation, testSending ]);
};

// Find out which protocol we're supposed to test, and which echo server we
// should be using, then kick off the tests.
chrome.test.sendMessage("info_please", onMessageReply);
