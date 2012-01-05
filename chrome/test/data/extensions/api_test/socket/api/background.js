// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const socket = chrome.experimental.socket;

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
      const message = "hello";
      var socketId = 0;
      var port = 0;

      function onRead(readInfo) {
        chrome.test.assertEq(message, readInfo.message);
        chrome.test.succeed();
      }

      function onWrite(writeInfo) {
        socket.read(socketId, onRead);
      }

      function onConnect(connectResult) {
        socket.write(socketId, message, onWrite);
      }

      function onCreate(socketInfo) {
        socketId = socketInfo.socketId;
        socket.connect(socketId, "127.0.0.1", port, onConnect);
      }

      chrome.test.sendMessage("port_please", function(message) {
          port = parseInt(message);
          socket.create("udp", {}, onCreate);
        });
    }
]);
