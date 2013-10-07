// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var listenOnce = chrome.test.listenOnce;
var listenForever = chrome.test.listenForever;

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
        url: "http://localhost:PORT/extensions/test_file.html"
                 .replace(/PORT/, config.testServer.port)
      }, function(newTab) {
        var doneListening = listenForever(chrome.tabs.onUpdated,
          function(_, info, tab) {
            if (tab.id == newTab.id && info.status == 'complete') {
              chrome.test.log("Created tab: " + tab.url);
              testTab = tab;
              doneListening();
            }
          });
      });
    },

    // Tests that postMessage to the tab and its response works.
    function postMessage() {
      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testPostMessage: true});
      listenOnce(port.onMessage, function(msg) {
        port.disconnect();
      });
    },

    // Tests that port name is sent & received correctly.
    function portName() {
      var portName = "lemonjello";
      var port = chrome.tabs.connect(testTab.id, {name: portName});
      port.postMessage({testPortName: true});
      listenOnce(port.onMessage, function(msg) {
        chrome.test.assertEq(msg.portName, portName);
        port.disconnect();
      });
    },

    // Tests that postMessage from the tab and its response works.
    function postMessageFromTab() {
      listenOnce(chrome.runtime.onConnect, function(port) {
        chrome.test.assertEq({
          tab: testTab,
          url: testTab.url,
           id: chrome.runtime.id
        }, port.sender);
        listenOnce(port.onMessage, function(msg) {
          chrome.test.assertTrue(msg.testPostMessageFromTab);
          port.postMessage({success: true, portName: port.name});
          chrome.test.log("postMessageFromTab: got message from tab");
        });
      });

      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testPostMessageFromTab: true});
      chrome.test.log("postMessageFromTab: sent first message to tab");
      listenOnce(port.onMessage, function(msg) {
        port.disconnect();
      });
    },

    // Tests receiving a request from a content script and responding.
    function sendMessageFromTab() {
      var doneListening = listenForever(
        chrome.runtime.onMessage,
        function(request, sender, sendResponse) {
          chrome.test.assertEq({
            tab: testTab,
            url: testTab.url,
             id: chrome.runtime.id
          }, sender);
          if (request.step == 1) {
            // Step 1: Page should send another request for step 2.
            chrome.test.log("sendMessageFromTab: got step 1");
            sendResponse({nextStep: true});
          } else {
            // Step 2.
            chrome.test.assertEq(request.step, 2);
            sendResponse();
            doneListening();
          }
      });

      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testSendMessageFromTab: true});
      port.disconnect();
      chrome.test.log("sendMessageFromTab: sent first message to tab");
    },

    // Tests error handling when sending a request from a content script to an
    // invalid extension.
    function sendMessageFromTabError() {
      listenOnce(
        chrome.runtime.onMessage,
        function(request, sender, sendResponse) {
          if (!request.success)
            chrome.test.fail();
        }
      );

      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testSendMessageFromTabError: true});
      port.disconnect();
      chrome.test.log("testSendMessageFromTabError: send 1st message to tab");
    },

    // Tests error handling when connecting to an invalid extension from a
    // content script.
    function connectFromTabError() {
      listenOnce(
        chrome.runtime.onMessage,
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
    function sendMessage() {
      chrome.tabs.sendMessage(testTab.id, {step2: 1}, function(response) {
        chrome.test.assertTrue(response.success);
        chrome.test.succeed();
      });
    },

    // Tests that we get the disconnect event when the tab disconnect.
    function disconnect() {
      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testDisconnect: true});
      listenOnce(port.onDisconnect, function() {});
    },

    // Tests that a message which fails to serialize prints an error and
    // doesn't send (http://crbug.com/263077).
    function unserializableMessage() {
      try {
        chrome.tabs.connect(testTab.id).postMessage(function() {
          // This shouldn't ever be called, so it's a bit pointless.
          chrome.test.fail();
        });
        // Didn't crash.
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e.stack);
      }
    },

    // Tests that we get the disconnect event when the tab context closes.
    function disconnectOnClose() {
      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testDisconnectOnClose: true});
      listenOnce(port.onDisconnect, function() {
        testTab = null; // the tab is about:blank now.
      });
    },

    // Tests that the sendRequest API is disabled.
    function sendRequest() {
      var error;
      try {
        chrome.extension.sendRequest("hi");
      } catch(e) {
        error = e;
      }
      chrome.test.assertTrue(error != undefined);

      error = undefined;
      try {
        chrome.extension.onRequest.addListener(function() {});
      } catch(e) {
        error = e;
      }
      chrome.test.assertTrue(error != undefined);

      chrome.test.succeed();
    },

  ]);
});
