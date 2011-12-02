// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function echoTest() {
    var ws = new WebSocket(
        "ws://localhost:8880/websocket/tests/hybi/workers/resources/echo");
    var MESSAGE_A = "message a";
    var MESSAGE_B = "message b";

    ws.onopen = function() {
      chrome.test.log("websocket opened.");
      ws.send(MESSAGE_A);
    };

    ws.onclose = function() {
      chrome.test.log("websocket closed.");
    }

    ws.onmessage = function(messageEvent) {
      chrome.test.log("message received: " + messageEvent.data);
      chrome.test.assertEq(MESSAGE_A, messageEvent.data);

      ws.onmessage = function(messageEvent) {
        chrome.test.log("message received: " + messageEvent.data);
        chrome.test.assertEq(MESSAGE_B, messageEvent.data);
        ws.close();

        chrome.test.succeed();
      };

      ws.send(MESSAGE_B);
    };
  }
]);
