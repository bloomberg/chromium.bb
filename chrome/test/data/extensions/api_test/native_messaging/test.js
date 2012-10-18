// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
    chrome.test.runTests([

      function sendMessageWithCallback() {
        var message = {"text": "Hi there!", "number": 3};
        chrome.extension.sendNativeMessage(
            'echo.py', message,
            chrome.test.callbackPass(function(nativeResponse) {
          var expectedResponse = {"id": 1, "echo": message};
          chrome.test.assertEq(expectedResponse, nativeResponse);
        }));
      },

      // The goal of this test, is just not to crash.
      function sendMessageWithoutCallback() {
        var message = {"text": "Hi there!", "number": 3};
        chrome.extension.sendNativeMessage('echo.py', message);
        chrome.test.succeed(); // Mission Complete
      },

      function connect() {
        var messagesToSend = [{"text": "foo"},
                              {"text": "bar", "funCount": 9001},
                              {}];
        var expectedResponses = [{"id": 1, "echo": messagesToSend[0]},
                                 {"id": 2, "echo": messagesToSend[1]},
                                 {"id": 3, "echo": messagesToSend[2]}];
        var currentMessage = 0;

        port = chrome.extension.connectNative('echo.py',
                                              messagesToSend[currentMessage]);
        port.onMessage.addListener(function(message) {
          chrome.test.assertEq(expectedResponses[currentMessage], message);
          currentMessage++;

          if (currentMessage == expectedResponses.length)
            chrome.test.notifyPass();
          else
            port.postMessage(messagesToSend[currentMessage]);
        });
      }
    ]);
});
