// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

var tabId;
var debuggee;
var protocolVersion = "0.1";

chrome.test.runTests([

  function attachMalformedVersion() {
    chrome.tabs.getSelected(null, function(tab) {
      chrome.experimental.debugger.attach({tabId: tab.id}, "malformed-version",
          fail("Requested protocol version is not supported: malformed-version."));
    });
  },

  function attachUnsupportedVersion() {
    chrome.tabs.getSelected(null, function(tab) {
      chrome.experimental.debugger.attach({tabId: tab.id}, "1.0",
          fail("Requested protocol version is not supported: 1.0."));
    });
  },

  function attach() {
    chrome.tabs.getSelected(null, function(tab) {
      tabId = tab.id;
      debuggee = {tabId: tab.id};
      chrome.experimental.debugger.attach(debuggee, protocolVersion, pass());
    });
  },

  function attachAgain() {
    chrome.experimental.debugger.attach(debuggee, protocolVersion,
        fail("Another debugger is already attached to the tab with id: " +
                 tabId + "."));
  },

  function sendCommand() {
    function onResponse() {
      if (chrome.extension.lastError &&
          chrome.extension.lastError.message.indexOf("invalidMethod") != -1)
        chrome.test.succeed();
      else
        chrome.test.fail();
    }
    chrome.experimental.debugger.sendCommand(debuggee,
                               "invalidMethod",
                               null,
                               onResponse);
  },

  function detach() {
    chrome.experimental.debugger.detach(debuggee, pass());
  },

  function sendCommandAfterDetach() {
    chrome.experimental.debugger.sendCommand(debuggee, "Foo", null,
        fail("Debugger is not attached to the tab with id: " + tabId + "."));
  },

  function detachAgain() {
    chrome.experimental.debugger.detach(debuggee,
        fail("Debugger is not attached to the tab with id: " + tabId + "."));
  },

  function closeTab() {
    chrome.tabs.create({url:"inspected.html"}, function(tab) {
      function onDetach(debuggee) {
        chrome.test.assertEq(tab.id, debuggee.tabId);
        chrome.experimental.debugger.onDetach.removeListener(onDetach);
        chrome.test.succeed();
      }

      var debuggee2 = {tabId: tab.id};
      chrome.experimental.debugger.attach(debuggee2, protocolVersion, function() {
        chrome.experimental.debugger.onDetach.addListener(onDetach);
        chrome.tabs.remove(tab.id);
      });
    });
  }
]);
