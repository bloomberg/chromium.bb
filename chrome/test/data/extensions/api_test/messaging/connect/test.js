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

    // Tests that reloading a child frame disconnects the port if it was the
    // only recipient of the port (i.e. no onConnect in main frame).
    function connectChildFrameAndNavigate() {
      listenOnce(chrome.runtime.onMessage, function(msg) {
        chrome.test.assertEq('testConnectChildFrameAndNavigateSetupDone', msg);
        // Now we have set up a frame and ensured that there is no onConnect
        // handler in the main frame. Run the actual test:
        var port = chrome.tabs.connect(testTab.id);
        listenOnce(port.onDisconnect, function() {});
        port.postMessage({testConnectChildFrameAndNavigate: true});
      });
      chrome.tabs.connect(testTab.id)
        .postMessage({testConnectChildFrameAndNavigateSetup: true});
    },

    // The previous test removed the onConnect listener. Add it back.
    function reloadTabForTest() {
      var doneListening = listenForever(chrome.tabs.onUpdated,
        function(tabId, info) {
          if (tabId === testTab.id && info.status == 'complete') {
            doneListening();
          }
        });
      chrome.tabs.reload(testTab.id);
    },

    // Tests that we get the disconnect event when the tab context closes.
    function disconnectOnClose() {
      listenOnce(chrome.runtime.onConnect, function(portFromTab) {
        listenOnce(portFromTab.onDisconnect, function() {
          chrome.test.assertNoLastError();
        });
        portFromTab.postMessage('unloadTabContent');
      });

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

    // Tests that chrome.runtime.sendMessage is *not* delivered to the current
    // context, consistent behavior with chrome.runtime.connect() and web APIs
    // like localStorage changed listeners.
    // Regression test for http://crbug.com/479951.
    function sendMessageToCurrentContextFails() {
      var stopFailing = failWhileListening(chrome.runtime.onMessage);
      chrome.runtime.sendMessage('ping', chrome.test.callbackFail(
          'Could not establish connection. Receiving end does not exist.',
          function() {
            stopFailing();
          }
      ));
    },

    // Like sendMessageToCurrentContextFails, but with the sendMessage call not
    // given a callback. This requires a more creative test setup because there
    // is no callback to signal when it's supposed to have been done.
    // Regression test for http://crbug.com/479951.
    //
    // NOTE(kalman): This test is correct. However, the patch which fixes it
    // (see bug) was reverted, and I don't plan on resubmitting, so instead
    // I'll comment out this test, and leave it here for the record.
    //
    // function sendMessageToCurrentTextWithoutCallbackFails() {
    //   // Make the iframe - in a different context - watch for the message
    //   // event. It *should* get it, while the current context's one doesn't.
    //   var iframe = document.createElement('iframe');
    //   iframe.src = chrome.runtime.getURL('blank_iframe.html');
    //   document.body.appendChild(iframe);

    //   var stopFailing = failWhileListening(chrome.runtime.onMessage);
    //   chrome.test.listenOnce(
    //     iframe.contentWindow.chrome.runtime.onMessage,
    //     function(msg, sender) {
    //       chrome.test.assertEq('ping', msg);
    //       chrome.test.assertEq(chrome.runtime.id, sender.id);
    //       chrome.test.assertEq(location.href, sender.url);
    //       setTimeout(function() {
    //         stopFailing();
    //       }, 0);
    //     }
    //   );
    //
    //   chrome.runtime.sendMessage('ping');
    // },
  ]);
});

function connectToTabWithFrameId(frameId, expectedMessages) {
  var port = chrome.tabs.connect(testTab.id, {
    frameId: frameId
  });
  var messages = [];
  var isDone = false;
  port.onMessage.addListener(function(message) {
    if (isDone) { // Should not get any messages after completing the test.
      chrome.test.fail(
          'Unexpected message from port to frame ' + frameId + ': ' + message);
      return;
    }

    messages.push(message);
    isDone = messages.length == expectedMessages.length;
    if (isDone) {
      chrome.test.assertEq(expectedMessages.sort(), messages.sort());
      chrome.test.succeed();
    }
  });
  port.onDisconnect.addListener(function() {
    if (!isDone) // The event should never be triggered when we expect messages.
      chrome.test.fail('Unexpected disconnect from port to frame ' + frameId);
  });
  port.postMessage({testSendMessageToFrame: true});
  chrome.test.log('connectToTabWithFrameId: port to frame ' + frameId);
}

// Listens to |event| and returns a callback to run to stop listening. While
// listening, if |event| is fired, calls chrome.test.fail().
function failWhileListening(event, doneListening) {
  var failListener = function() {
    chrome.test.fail('Event listener ran, but it shouldn\'t have. ' +
                     'It\'s possible that may be triggered flakily, but this ' +
                     'really is a real failure, not flaky sadness. Promise!');
  };
  var release = chrome.test.callbackAdded();
  event.addListener(failListener);
  return function() {
    event.removeListener(failListener);
    release();
  };
}
