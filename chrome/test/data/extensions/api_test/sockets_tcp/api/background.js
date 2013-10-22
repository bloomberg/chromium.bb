// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// net/tools/testserver/testserver.py is picky about the format of what it
// calls its "echo" messages. One might go so far as to mutter to oneself that
// it isn't an echo server at all.
//
// The response is based on the request but obfuscated using a random key.
const request = "0100000005320000005hello";
var expectedResponsePattern = /0100000005320000005.{11}/;

var address;
var bytesSent = 0;
var dataAsString;
var dataRead = [];
var port = -1;
var protocol = "none";
var socketId = 0;
var echoDataSent = false;
var succeeded = false;
var waitCount = 0;

// Many thanks to Dennis for his StackOverflow answer: http://goo.gl/UDanx
// Since amended to handle BlobBuilder deprecation.
function string2ArrayBuffer(string, callback) {
  var blob = new Blob([string]);
  var f = new FileReader();
  f.onload = function(e) {
    callback(e.target.result);
  };
  f.readAsArrayBuffer(blob);
}

function arrayBuffer2String(buf, callback) {
  var blob = new Blob([new Uint8Array(buf)]);
  var f = new FileReader();
  f.onload = function(e) {
    callback(e.target.result);
  };
  f.readAsText(blob);
}

var testSocketCreation = function() {
  function onCreate(socketInfo) {
    function onGetInfo(info) {
      chrome.test.assertFalse(info.connected);

      if (info.peerAddress || info.peerPort) {
        chrome.test.fail('Unconnected socket should not have peer');
      }
      if (info.localAddress || info.localPort) {
        chrome.test.fail('Unconnected socket should not have local binding');
      }

      chrome.sockets.tcp.close(socketInfo.socketId, function() {
        chrome.sockets.tcp.getInfo(socketInfo.socketId, function(info) {
          chrome.test.assertEq(undefined, info);
          chrome.test.succeed();
        });
      });
    }

    chrome.test.assertTrue(socketInfo.socketId > 0);

    // Obtaining socket information before a connect() call should be safe, but
    // return empty values.
    chrome.sockets.tcp.getInfo(socketInfo.socketId, onGetInfo);
  }

  chrome.sockets.tcp.create({}, onCreate);
};


var testSending = function() {
  dataRead = "";
  succeeded = false;
  echoDataSent = false;
  waitCount = 0;

  setTimeout(waitForBlockingOperation, 1000);

  createSocket();

  function createSocket() {
    chrome.sockets.tcp.create({
      "name": "test",
      "persistent": true,
      "bufferSize": 104
    }, onCreateComplete);
  }

  function onCreateComplete(socketInfo) {
    console.log("onCreateComplete");
    socketId = socketInfo.socketId;
    chrome.test.assertTrue(socketId > 0, "failed to create socket");

    console.log("add event listeners");
    chrome.sockets.tcp.onReceive.addListener(onSocketReceive);
    chrome.sockets.tcp.onReceiveError.addListener(onSocketReceiveError);

    chrome.sockets.tcp.getInfo(socketId, onGetInfoAfterCreateComplete);
  }

  function onGetInfoAfterCreateComplete(result) {
    console.log("onGetInfoAfterCreateComplete");
    chrome.test.assertTrue(!result.localAddress,
                           "Socket should not have local address");
    chrome.test.assertTrue(!result.localPort,
                           "Socket should not have local port");
    chrome.test.assertTrue(!result.peerAddress,
                           "Socket should not have peer address");
    chrome.test.assertTrue(!result.peerPort,
                           "Socket should not have peer port");
    chrome.test.assertFalse(result.connected, "Socket should not be connected");

    chrome.test.assertEq("test", result.name, "Socket name did not persist");
    chrome.test.assertTrue(result.persistent,
                           "Socket should be persistent");
    chrome.test.assertEq(104, result.bufferSize, "Buffer size did not persist");
    chrome.test.assertFalse(result.paused, "Socket should not be paused");

    chrome.sockets.tcp.update(socketId, {
      "name": "test2",
      "persistent": false,
      bufferSize: 2048
    }, onUpdateComplete);
  }

  function onUpdateComplete() {
    console.log("onUpdateComplete");
    chrome.sockets.tcp.getInfo(socketId, onGetInfoAfterUpdateComplete);
  }

  function onGetInfoAfterUpdateComplete(result) {
    console.log("onGetInfoAfterUpdateComplete");
    chrome.test.assertTrue(!result.localAddress,
                           "Socket should not have local address");
    chrome.test.assertTrue(!result.localPort,
                           "Socket should not have local port");
    chrome.test.assertTrue(!result.peerAddress,
                           "Socket should not have peer address");
    chrome.test.assertTrue(!result.peerPort,
                           "Socket should not have peer port");
    chrome.test.assertFalse(result.connected, "Socket should not be connected");

    chrome.test.assertEq("test2", result.name, "Socket name did not persist");
    chrome.test.assertFalse(result.persistent,
                           "Socket should not be persistent");
    chrome.test.assertEq(2048, result.bufferSize,
                         "Buffer size did not persist");
    chrome.test.assertFalse(result.paused, "Socket should not be paused");

    chrome.sockets.tcp.connect(socketId, address, port, onConnectComplete);
  }

  function onConnectComplete(result) {
    console.log("onConnectComplete");
    chrome.test.assertEq(0, result,
                         "Connect failed with error " + result);

    chrome.sockets.tcp.getInfo(socketId, onGetInfoAfterConnectComplete);
  }

  function onGetInfoAfterConnectComplete(result) {
    console.log("onGetInfoAfterConnectComplete");
    chrome.test.assertTrue(!!result.localAddress,
                           "Bound socket should always have local address");
    chrome.test.assertTrue(!!result.localPort,
                           "Bound socket should always have local port");

    // NOTE: We're always called with 'localhost', but getInfo will only return
    // IPs, not names.
    chrome.test.assertEq(result.peerAddress, "127.0.0.1",
                         "Peer addresss should be the listen server");
    chrome.test.assertEq(result.peerPort, port,
                         "Peer port should be the listen server");
    chrome.test.assertTrue(result.connected, "Socket should be connected");

    chrome.sockets.tcp.setPaused(socketId, true, onSetPausedComplete);
  }

  function onSetPausedComplete() {
    console.log("onSetPausedComplete");
    chrome.sockets.tcp.getInfo(socketId, onGetInfoAfterSetPausedComplete);
  }

  function onGetInfoAfterSetPausedComplete(result) {
    console.log("onGetInfoAfterSetPausedComplete");
    chrome.test.assertTrue(result.paused, "Socket should be paused");
    chrome.sockets.tcp.setPaused(socketId, false, onUnpauseComplete);
  }

  function onUnpauseComplete() {
    console.log("onUnpauseComplete");
    chrome.sockets.tcp.getInfo(socketId, onGetInfoAfterUnpauseComplete);
  }

  function onGetInfoAfterUnpauseComplete(result) {
    console.log("onGetInfoAfterUnpauseComplete");
    chrome.test.assertFalse(result.paused, "Socket should not be paused");
    chrome.sockets.tcp.setNoDelay(socketId, true, onSetNoDelayComplete);
  }

  function onSetNoDelayComplete(result) {
    console.log("onSetNoDelayComplete");
    if (result != 0) {
      chrome.test.fail("setNoDelay failed for TCP: " +
          "result=" + result + ", " +
          "lastError=" + chrome.runtime.lastError.message);
    }
    chrome.sockets.tcp.setKeepAlive(
        socketId, true, 1000, onSetKeepAliveComplete);
  }

  function onSetKeepAliveComplete(result) {
    console.log("onSetKeepAliveComplete");
    if (result != 0) {
      chrome.test.fail("setKeepAlive failed for TCP: " +
          "result=" + result + ", " +
          "lastError=" + chrome.runtime.lastError.message);
    }

    string2ArrayBuffer(request, function(arrayBuffer) {
      echoDataSent = true;
      chrome.sockets.tcp.send(socketId, arrayBuffer, onSendComplete);
    });
  }

  function onSendComplete(sendInfo) {
    console.log("onSendComplete: " + sendInfo.bytesSent + " bytes.");
    chrome.test.assertEq(0, sendInfo.resultCode, "Send failed.");
    chrome.test.assertTrue(sendInfo.bytesSent > 0,
        "Send didn't write bytes.");
    bytesSent += sendInfo.bytesSent;
  }

  function onSocketReceive(receiveInfo) {
    console.log("onSocketReceive");
    chrome.test.assertEq(socketId, receiveInfo.socketId);
    arrayBuffer2String(receiveInfo.data, function(s) {
      dataAsString = s;  // save this for error reporting
      var match = !!s.match(expectedResponsePattern);
      chrome.test.assertTrue(match, "Received data does not match.");
      console.log("echo data received, closing socket");
      chrome.sockets.tcp.close(socketId, onCloseComplete);
    });
  }

  function onSocketReceiveError(receiveErrorInfo) {
    // Note: Once we have sent the "echo" message, the echo server sends back
    // the "echo" response and closes the connection right away. This means
    // we get a "connection closed" error very quickly after sending our
    // message. This is why we ignore errors from that point on.
    if (echoDataSent)
      return;

    console.log("onSocketReceiveError");
    chrome.test.fail("Receive error on socket " + receiveErrorInfo.socketId +
      ": result code=" + receiveErrorInfo.resultCode);
  }

  function onCloseComplete() {
    console.log("onCloseComplete");
    succeeded = true;
    chrome.test.succeed();
  }

};  // testSending()

function waitForBlockingOperation() {
  if (++waitCount < 10) {
    setTimeout(waitForBlockingOperation, 1000);
  } else {
    // We weren't able to succeed in the given time.
    chrome.test.fail("Operations didn't complete after " + waitCount + " " +
                     "seconds. Response so far was <" + dataAsString + ">.");
  }
}

var onMessageReply = function(message) {
  var parts = message.split(":");
  var test_type = parts[0];
  address = parts[1];
  port = parseInt(parts[2]);
  console.log("Running tests, protocol " + test_type + ", echo server " +
              address + ":" + port);
  if (test_type == 'tcp') {
    protocol = test_type;
    chrome.test.runTests([testSocketCreation, testSending]);
  } else {
    chrome.test.fail("Invalid test type: " + test_type);
  }
};

// Find out which protocol we're supposed to test, and which echo server we
// should be using, then kick off the tests.
chrome.test.sendMessage("info_please", onMessageReply);
