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

const socket = chrome.sockets.udp;
var address;
var bytesWritten = 0;
var dataAsString;
var dataRead = [];
var port = -1;
var socketId = 0;
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

      socket.close(createInfo.socketId, function() {
        socket.getInfo(createInfo.socketId, function(info) {
          chrome.test.assertEq(undefined, info);
          chrome.test.succeed();
        });
      });
    }

    chrome.test.assertTrue(createInfo.socketId > 0);

    // Obtaining socket information before a connect() call should be safe, but
    // return empty values.
    socket.getInfo(createInfo.socketId, onGetInfo);
  }

  socket.create({}, onCreate);
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

  setTimeout(waitForBlockingOperation, 1000);
  socket.create({}, function (socketInfo) {
    console.log("socket created");
    socketId = socketInfo.socketId;
    chrome.test.assertTrue(socketId > 0, "failed to create socket");
    socket.bind(socketId, "0.0.0.0", 0, function (result) {
      console.log("socket bound to local host");
      chrome.test.assertEq(0, result,
                           "Connect or bind failed with error " + result);
      if (result == 0) {
        socket.getInfo(socketId, function (result) {
          console.log("got socket info");
          chrome.test.assertTrue(
              !!result.localAddress,
              "Bound socket should always have local address");
          chrome.test.assertTrue(
              !!result.localPort,
              "Bound socket should always have local port");

          string2ArrayBuffer(request, function(arrayBuffer) {
            socket.onReceiveError.addListener(function (info) {
              chrome.test.fail("Socket receive error:" + info.result);
            });
            socket.onReceive.addListener(function (info) {
              console.log(
                  "received bytes from echo server: " + info.data.byteLength);
              if (socketId == info.socketId) {
                arrayBuffer2String(info.data, function(s) {
                    dataAsString = s;  // save this for error reporting
                    var match = !!s.match(expectedResponsePattern);
                    chrome.test.assertTrue(
                        match, "Received data does not match.");
                    succeeded = true;
                    chrome.test.succeed();
                });
              }
            });
            console.log(
                "sending bytes to echo server: " + arrayBuffer.byteLength);
            socket.send(socketId, arrayBuffer, address, port,
                function(writeInfo) {
              chrome.test.assertEq(0, writeInfo.result);
              chrome.test.assertEq(
                  writeInfo.bytesWritten, arrayBuffer.byteLength);
            });
          });
        });
      }
    });
  });
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
    chrome.test.runTests([ testSocketCreation, testSending ]);
  }
};

// Find out which protocol we're supposed to test, and which echo server we
// should be using, then kick off the tests.
chrome.test.sendMessage("info_please", onMessageReply);
