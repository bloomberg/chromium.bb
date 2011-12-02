// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

JSON.parse = function() {
  return "JSON.parse clobbered by extension.";
};

JSON.stringify = function() {
  return "JSON.stringify clobbered by extension.";
};

Array.prototype.toJSON = function() {
  return "Array.prototype.toJSON clobbered by extension.";
};

Object.prototype.toJSON = function() {
  return "Object.prototype.toJSON clobbered by extension.";
};

// Keep track of the tab that we're running tests in, for simplicity.
var testTab = null;

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function setupTestTab() {
      chrome.test.log("Creating tab...");
      chrome.tabs.create({
        url: "http://localhost:PORT/files/extensions/test_file.html"
                 .replace(/PORT/, config.testServer.port)
      }, function(tab) {
        chrome.tabs.onUpdated.addListener(function listener(tabid, info) {
          if (tab.id == tabid && info.status == 'complete') {
            chrome.test.log("Created tab: " + tab.url);
            chrome.tabs.onUpdated.removeListener(listener);
            testTab = tab;
            chrome.test.succeed();
          }
        });
      });
    },

    // Tests that postMessage to the tab and its response works.
    function postMessage() {
      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testPostMessage: true});
      port.onMessage.addListener(function(msg) {
        port.disconnect();
        chrome.test.succeed();
      });
    },

    // Tests that port name is sent & received correctly.
    function portName() {
      var portName = "lemonjello";
      var port = chrome.tabs.connect(testTab.id, {name: portName});
      port.postMessage({testPortName: true});
      port.onMessage.addListener(function(msg) {
        chrome.test.assertEq(msg.portName, portName);
        port.disconnect();
        chrome.test.succeed();
      });
    },

    // Tests that postMessage from the tab and its response works.
    function postMessageFromTab() {
      chrome.extension.onConnect.addListener(function(port) {
        port.onMessage.addListener(function(msg) {
          chrome.test.assertTrue(msg.testPostMessageFromTab);
          port.postMessage({success: true, portName: port.name});
          chrome.test.log("postMessageFromTab: got message from tab");
        });
      });

      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testPostMessageFromTab: true});
      chrome.test.log("postMessageFromTab: sent first message to tab");
      port.onMessage.addListener(function(msg) {
        port.disconnect();
        chrome.test.succeed();
      });
    },

    // Tests receiving a request from a content script and responding.
    function sendRequestFromTab() {
      var doneListening = chrome.test.listenForever(
        chrome.extension.onRequest,
        function(request, sender, sendResponse) {
          chrome.test.assertTrue("url" in sender.tab, "no tab available.");
          chrome.test.assertEq(sender.id, location.host);
          if (request.step == 1) {
            // Step 1: Page should send another request for step 2.
            chrome.test.log("sendRequestFromTab: got step 1");
            sendResponse({nextStep: true});
          } else {
            // Step 2.
            chrome.test.assertEq(request.step, 2);
            sendResponse();
            doneListening();
          }
      });

      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testSendRequestFromTab: true});
      port.disconnect();
      chrome.test.log("sendRequestFromTab: sent first message to tab");
    },

    // Tests error handling when sending a request from a content script to an
    // invalid extension.
    function sendRequestFromTabError() {
      chrome.test.listenOnce(
        chrome.extension.onRequest,
        function(request, sender, sendResponse) {
          if (!request.success)
            chrome.test.fail();
        }
      );

      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testSendRequestFromTabError: true});
      port.disconnect();
      chrome.test.log("testSendRequestFromTabError: send 1st message to tab");
    },

    // Tests error handling when connecting to an invalid extension from a
    // content script.
    function connectFromTabError() {
      chrome.test.listenOnce(
        chrome.extension.onRequest,
        function(request, sender, sendResponse) {
          if (!request.success)
            chrome.test.fail();
        }
      );

      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testConnectFromTabError: true});
      port.disconnect();
      chrome.test.log("testConnectFromTabError: sent 1st message to tab");
    },

    // Tests sending a request to a tab and receiving a response.
    function sendRequest() {
      chrome.tabs.sendRequest(testTab.id, {step2: 1}, function(response) {
        chrome.test.assertTrue(response.success);
        chrome.test.succeed();
      });
    },

    // Tests that we get the disconnect event when the tab disconnect.
    function disconnect() {
      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testDisconnect: true});
      port.onDisconnect.addListener(function() {
        chrome.test.succeed();
      });
    },

    // Tests that we get the disconnect event when the tab context closes.
    function disconnectOnClose() {
      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testDisconnectOnClose: true});
      port.onDisconnect.addListener(function() {
        chrome.test.succeed();
        testTab = null; // the tab is about:blank now.
      });
    },
  ]);
});
