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
      var newTab = null;
      var doneListening = listenForever(chrome.tabs.onUpdated,
        function(_, info, tab) {
          if (newTab && tab.id == newTab.id && info.status == 'complete') {
            chrome.test.log("Created tab: " + tab.url);
            testTab = tab;
            doneListening();
          }
        });
      chrome.tabs.create({
        url: "http://localhost:PORT/extensions/test_file.html"
                 .replace(/PORT/, config.testServer.port)
      }, function(tab) {
        newTab = tab;
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
          frameId: 0, // Main frame
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
            frameId: 0, // Main frame
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

    // Tests that a message from a child frame has a correct frameId.
    function sendMessageFromFrameInTab() {
      var senders = [];
      var doneListening = listenForever(
        chrome.runtime.onMessage,
        function(request, sender, sendResponse) {
          // The tab's load status could either be "loading" or "complete",
          // depending on whether all frames have finished loading. Since we
          // want this test to be deterministic, set status to "complete".
          sender.tab.status = 'complete';
          // Child frames have a positive frameId.
          senders.push(sender);

          // testSendMessageFromFrame() in page.js adds 2 frames. Wait for
          // messages from each.
          if (senders.length == 2) {
            chrome.webNavigation.getAllFrames({
              tabId: testTab.id
            }, function(details) {
              function sortByFrameId(a, b) {
                return a.frameId < b.frameId ? 1 : -1;
              }
              var expectedSenders = details.filter(function(frame) {
                return frame.frameId > 0; // Exclude main frame.
              }).map(function(frame) {
                return {
                  tab: testTab,
                  frameId: frame.frameId,
                  url: frame.url,
                  id: chrome.runtime.id
                };
              }).sort(sortByFrameId);
              senders.sort(sortByFrameId);
              chrome.test.assertEq(expectedSenders, senders);
              doneListening();
            });
          }
        }
      );

      var port = chrome.tabs.connect(testTab.id);
      port.postMessage({testSendMessageFromFrame: true});
      port.disconnect();
      chrome.test.log("sendMessageFromFrameInTab: send 1st message to tab");
    },

    // connect to frameId 0 should trigger onConnect in the main frame only.
    function sendMessageToMainFrameInTab() {
      connectToTabWithFrameId(0, ['from_main']);
    },

    // connect without frameId should trigger onConnect in every frame.
    function sendMessageToAllFramesInTab() {
      connectToTabWithFrameId(undefined, ['from_main', 'from_0', 'from_1']);
    },

    // connect with a positive frameId should trigger onConnect in that specific
    // frame only.
    function sendMessageToFrameInTab() {
      chrome.webNavigation.getAllFrames({
        tabId: testTab.id
      }, function(details) {
        var frames = details.filter(function(frame) {
          return /\?testSendMessageFromFrame1$/.test(frame.url);
        });
        chrome.test.assertEq(1, frames.length);
        connectToTabWithFrameId(frames[0].frameId, ['from_1']);
      });
    },

    // sendMessage with an invalid frameId should fail.
    function sendMessageToInvalidFrameInTab() {
      chrome.tabs.sendMessage(testTab.id, {}, {
        frameId: 999999999 // Some (hopefully) invalid frameId.
      }, chrome.test.callbackFail(
        'Could not establish connection. Receiving end does not exist.'));
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

function connectToTabWithFrameId(frameId, expectedMessages) {
  var port = chrome.tabs.connect(testTab.id, {
    frameId: frameId
  });
  var messages = [];
  var isDone = false;
  listenForever(port.onMessage, function(message) {
    if (isDone) // Should not get any messages after completing the test.
      chrome.test.fail(
          'Unexpected message from port to frame ' + frameId + ': ' + message);

    messages.push(message);
    isDone = messages.length == expectedMessages.length;
    if (isDone) {
      chrome.test.assertEq(expectedMessages.sort(), messages.sort());
      chrome.test.succeed();
    }
  });
  listenOnce(port.onDisconnect, function() {
    if (!isDone) // The event should never be triggered when we expect messages.
      chrome.test.fail('Unexpected disconnect from port to frame ' + frameId);
  });
  port.postMessage({testSendMessageToFrame: true});
  chrome.test.log('connectToTabWithFrameId: port to frame ' + frameId);
}
