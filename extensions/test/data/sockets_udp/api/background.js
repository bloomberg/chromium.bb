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
var socketId = 0;
var succeeded = false;
var waitCount = 0;

// Many thanks to Dennis for the StackOverflow answer: http://goo.gl/UDanx
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

///////////////////////////////////////////////////////////////////////////////
// Test socket creation
//

var testSocketCreation = function() {
  function onCreate(createInfo) {
    function onGetInfo(info) {
      if (info.localAddress || info.localPort) {
        chrome.test.fail('Unconnected socket should not have local binding');
      }

      chrome.test.assertEq(createInfo.socketId, info.socketId);
      chrome.test.assertEq(false, info.persistent);

      chrome.sockets.udp.close(createInfo.socketId, function() {
        chrome.sockets.udp.getInfo(createInfo.socketId, function(info) {
          chrome.test.assertEq(undefined, info);
          chrome.test.succeed();
        });
      });
    }

    chrome.test.assertTrue(createInfo.socketId > 0);

    // Obtaining socket information before a connect() call should be safe, but
    // return empty values.
    chrome.sockets.udp.getInfo(createInfo.socketId, onGetInfo);
  }

  chrome.sockets.udp.create({}, onCreate);
};

///////////////////////////////////////////////////////////////////////////////
// Test socket send/receive
//

function waitForBlockingOperation() {
  if (++waitCount < 10) {
    setTimeout(waitForBlockingOperation, 1000);
  } else {
    // We weren't able to succeed in the given time.
    chrome.test.fail("Operations didn't complete after " + waitCount + " " +
                     "seconds. Response so far was <" + dataAsString + ">.");
  }
}

var testSending = function() {
  dataRead = "";
  succeeded = false;
  waitCount = 0;
  socketId = 0;
  var localSocketId;

  console.log("testSending");
  setTimeout(waitForBlockingOperation, 1000);
  chrome.sockets.udp.create({}, onCreate);

  function onCreate(socketInfo) {
    console.log("socket created: " + socketInfo.socketId);
    localSocketId = socketId = socketInfo.socketId;
    chrome.test.assertTrue(localSocketId > 0, "failed to create socket");
    chrome.sockets.udp.onReceive.addListener(onReceive);
    chrome.sockets.udp.onReceiveError.addListener(onReceiveError);
    chrome.sockets.udp.bind(localSocketId, "0.0.0.0", 0, onBind);
  }

  function onBind(result) {
    console.log("socket bound to local host");
    chrome.test.assertEq(0, result, "Bind failed with error: " + result);
    if (result < 0)
      return;

    chrome.sockets.udp.getInfo(localSocketId, onGetInfo);
  }

  function onGetInfo(result) {
    console.log("got socket info");
    chrome.test.assertTrue(!!result.localAddress,
                           "Bound socket should always have local address");
    chrome.test.assertTrue(!!result.localPort,
                           "Bound socket should always have local port");

    string2ArrayBuffer(request, onArrayBuffer);
  }

  function onArrayBuffer(arrayBuffer) {
    console.log("sending bytes to echo server: " + arrayBuffer.byteLength);
    chrome.sockets.udp.send(localSocketId, arrayBuffer, address, port,
                            function(sendInfo) {
      chrome.test.assertEq(0, sendInfo.resultCode);
      chrome.test.assertEq(sendInfo.bytesSent, arrayBuffer.byteLength);
    });
  }

  function onReceiveError(info) {
    chrome.test.fail("Socket receive error: " + info.resultCode);
  }

  function onReceive(info) {
    console.log("received bytes on from echo server: " + info.data.byteLength +
      "(" + info.socketId + ")");
    if (localSocketId == info.socketId) {
      arrayBuffer2String(info.data, function(s) {
        dataAsString = s;  // save this for error reporting
        var match = !!s.match(expectedResponsePattern);
        chrome.test.assertTrue(match, "Received data does not match.");
        chrome.sockets.udp.close(localSocketId, function () {
          chrome.sockets.udp.onReceive.removeListener(onReceive);
          chrome.sockets.udp.onReceiveError.removeListener(onReceiveError);
          succeeded = true;
          chrome.test.succeed();
        });
      });
    }
  }
}

var testSetPaused = function() {
  dataRead = "";
  succeeded = false;
  waitCount = 0;
  socketId = 0;
  var localSocketId = 0;
  var receiveTimer;

  console.log("testSetPaused");
  setTimeout(waitForBlockingOperation, 1000);
  chrome.sockets.udp.create({}, onCreate);

  function onCreate(socketInfo) {
    console.log("socket created: " + socketInfo.socketId);
    localSocketId = socketId = socketInfo.socketId;
    chrome.test.assertTrue(localSocketId > 0, "failed to create socket");

    chrome.sockets.udp.onReceiveError.addListener(onReceiveError);
    chrome.sockets.udp.onReceive.addListener(onReceive);

    chrome.sockets.udp.setPaused(localSocketId, true, function () {
      chrome.sockets.udp.bind(localSocketId, "0.0.0.0", 0, onBind);
    });
  }

  function onBind(result) {
    console.log("socket bound to local host");
    chrome.test.assertEq(0, result, "Bind failed with error: " + result);
    if (result < 0)
      return;

    string2ArrayBuffer(request, onArrayBuffer);
  }

  function onArrayBuffer(arrayBuffer) {
    console.log("sending bytes to echo server: " + arrayBuffer.byteLength);
    chrome.sockets.udp.send(localSocketId, arrayBuffer, address, port,
                            function(sendInfo) {
      chrome.test.assertEq(0, sendInfo.resultCode);
      chrome.test.assertEq(sendInfo.bytesSent, arrayBuffer.byteLength);
      receiveTimer = setTimeout(waitForReceiveEvents, 1000);
    });
  }

  function waitForReceiveEvents() {
    chrome.sockets.udp.close(localSocketId, function () {
      chrome.sockets.udp.onReceive.removeListener(onReceive);
      chrome.sockets.udp.onReceiveError.removeListener(onReceiveError);
      succeeded = true;
      chrome.test.succeed("No data received from echo server!");
    });
  };

  function onReceiveError(info) {
    if (localSocketId == info.socketId) {
      if (receiveTimer)
        clearTimeout(receiveTimer);
      chrome.test.fail("Socket receive error: " + info.resultCode);
    }
  }

  function onReceive(info) {
    console.log("Received data on socket" + "(" + info.socketId + ")");
    if (localSocketId == info.socketId) {
      if (receiveTimer)
        clearTimeout(receiveTimer);
      chrome.test.fail("Should not receive data when socket is paused: " +
                       info.data.byteLength);
    }
  }
}

var testBroadcast = function() {
  var listeningSocketId;
  var sendingSocketId;

  console.log("testBroadcast");
  chrome.sockets.udp.create({}, onCreate);

  function onCreate(socketInfo) {
    console.log("socket created: " + socketInfo.socketId);
    chrome.test.assertTrue(socketInfo.socketId > 0, "failed to create socket");

    if (listeningSocketId == undefined) {
      listeningSocketId = socketInfo.socketId;
      chrome.sockets.udp.onReceive.addListener(onReceive);
      chrome.sockets.udp.onReceiveError.addListener(onReceiveError);
      chrome.sockets.udp.bind(
          listeningSocketId, "0.0.0.0", 8000, onBindListening);
    } else {
      sendingSocketId = socketInfo.socketId;
      chrome.sockets.udp.bind(
          sendingSocketId, "127.0.0.1", 8001, onBindSending);
    }
  }

  function onBindListening(result) {
    chrome.test.assertEq(0, result, "Bind failed with error: " + result);
    if (result < 0) {
      return;
    }

    chrome.sockets.udp.setBroadcast(
        listeningSocketId, true, onSetBroadcastListening);
  }

  function onSetBroadcastListening(result) {
    chrome.test.assertEq(0, result, "Failed to enable broadcast: " + result);
    if (result < 0) {
      return;
    }

    // Create the sending socket.
    chrome.sockets.udp.create({}, onCreate);
  }

  function onBindSending(result) {
    chrome.test.assertEq(0, result, "Bind failed with error: " + result);
    if (result < 0) {
      return;
    }

    chrome.sockets.udp.setBroadcast(
        sendingSocketId, true, onSetBroadcastSending);
  }

  function onSetBroadcastSending(result) {
    chrome.test.assertEq(0, result, "Failed to enable broadcast: " + result);
    if (result < 0) {
      return;
    }

    string2ArrayBuffer("broadcast packet", onArrayBuffer);
  }

  function onArrayBuffer(arrayBuffer) {
    console.log("sending bytes to broadcast: " + arrayBuffer.byteLength);
    chrome.sockets.udp.send(sendingSocketId, arrayBuffer, "127.255.255.255",
                            8000, function(sendInfo) {
      chrome.test.assertEq(0, sendInfo.resultCode);
      chrome.test.assertEq(sendInfo.bytesSent, arrayBuffer.byteLength);
    });
  }

  function onReceiveError(info) {
    chrome.test.fail("Socket receive error: " + info.resultCode);
  }

  function onReceive(info) {
    console.log("Received data on socket" + "(" + info.socketId + ")");
    chrome.test.assertEq(listeningSocketId, info.socketId);
    chrome.test.assertEq("127.0.0.1", info.remoteAddress);
    chrome.test.assertEq(8001, info.remotePort);
    arrayBuffer2String(info.data, function(string) {
      chrome.test.assertEq("broadcast packet", string);
      chrome.test.succeed();
    });
  }
};

///////////////////////////////////////////////////////////////////////////////
// Test driver
//

var onMessageReply = function(message) {
  var parts = message.split(":");
  var test_type = parts[0];
  address = parts[1];
  port = parseInt(parts[2]);
  console.log("Running tests, echo server " +
              address + ":" + port);
  if (test_type == 'multicast') {
    console.log("Running multicast tests");
    chrome.test.runTests([ testMulticast ]);
  } else {
    console.log("Running udp tests");
    chrome.test.runTests([ testSocketCreation, testSending, testSetPaused,
                           testBroadcast ]);
  }
};

// Find out which protocol we're supposed to test, and which echo server we
// should be using, then kick off the tests.
chrome.test.sendMessage("info_please", onMessageReply);
