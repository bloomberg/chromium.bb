// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const socket = chrome.experimental.socket;

var sendQuit = function(socketId) {
  if (socketId) {
    socket.write(socketId, "QUIT", function(writeInfo) {});
  }
}

chrome.test.runTests([
    function testCreation() {
      function onCreate(socketInfo) {
        chrome.test.assertTrue(socketInfo.socketId > 0);

        // TODO(miket): this doesn't work yet. It's possible this will become
        // automatic, but either way we can't forget to clean up.
        //socket.destroy(socketInfo.socketId);

        chrome.test.succeed();
      }

      socket.create("udp", {}, onCreate);
    },

    function testSending() {
      const message = "helloECHO";
      var dataRead = "";
      var socketId = 0;
      var port = 0;
      var succeeded = false;
      var waitCount = 0;

      function onRead(readInfo) {
        if (readInfo.message == message) {
          succeeded = true;
          sendQuit(socketId);
          chrome.test.succeed();
        } else {
          // The read blocked. Save what we've got so far, and wait for
          // onEvent.
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

      function onEvent(socketEvent) {
        if (socketEvent.type == "dataRead") {
          dataRead += socketEvent.data;
          if (dataRead == message) {
            succeeded = true;
            sendQuit(socketId);
            chrome.test.succeed();
          } else {
            // We got only some of the message. Keep waiting.
          }
        } else {
          console.log("Received other socketEvent of type " +
                      socketEvent.type);
        }
      }

      function onCreate(socketInfo) {
        socketId = socketInfo.socketId;
        chrome.test.assertTrue(socketId > 0);
        socket.connect(socketId, "127.0.0.1", port, onConnect);
      }

      function waitForBlockingRead() {
        if (waitCount++ < 10) {
          setTimeout(waitForBlockingRead, 1000);
        } else {
          // We weren't able to succeed in the given time.
          sendQuit(socketId);
          chrome.test.fail(
              "Blocking read didn't complete after " + waitCount +
              " seconds. Message so far was <" + dataRead + ">.");
        }
      }

      chrome.test.sendMessage("port_please", function(message) {
          port = parseInt(message);
          socket.create("udp", { onEvent: onEvent }, onCreate);
        });
      setTimeout(waitForBlockingRead, 1000);
    }
]);
