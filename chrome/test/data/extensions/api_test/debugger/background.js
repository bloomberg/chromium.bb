// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

var tabId;
var debuggee;
var protocolVersion = "1.0";

chrome.test.runTests([

  function attachMalformedVersion() {
    chrome.tabs.getSelected(null, function(tab) {
      chrome.debugger.attach({tabId: tab.id}, "malformed-version",
          fail("Requested protocol version is not supported: malformed-version."));
    });
  },

  function attachUnsupportedMinorVersion() {
    chrome.tabs.getSelected(null, function(tab) {
      chrome.debugger.attach({tabId: tab.id}, "1.5",
          fail("Requested protocol version is not supported: 1.5."));
    });
  },

  function attachUnsupportedVersion() {
    chrome.tabs.getSelected(null, function(tab) {
      chrome.debugger.attach({tabId: tab.id}, "100.0",
          fail("Requested protocol version is not supported: 100.0."));
    });
  },

  function attach() {
    chrome.tabs.getSelected(null, function(tab) {
      tabId = tab.id;
      debuggee = {tabId: tab.id};
      chrome.debugger.attach(debuggee, protocolVersion, pass());
    });
  },

  function attachAgain() {
    chrome.debugger.attach(debuggee, protocolVersion,
        fail("Another debugger is already attached to the tab with id: " +
                 tabId + "."));
  },

  function sendCommand() {
    function onResponse() {
      if (chrome.runtime.lastError &&
          chrome.runtime.lastError.message.indexOf("invalidMethod") != -1)
        chrome.test.succeed();
      else
        chrome.test.fail();
    }
    chrome.debugger.sendCommand(debuggee,
                               "DOM.invalidMethod",
                               null,
                               onResponse);
  },

  function detach() {
    chrome.debugger.detach(debuggee, pass());
  },

  function sendCommandAfterDetach() {
    chrome.debugger.sendCommand(debuggee, "Foo", null,
        fail("Debugger is not attached to the tab with id: " + tabId + "."));
  },

  function detachAgain() {
    chrome.debugger.detach(debuggee,
        fail("Debugger is not attached to the tab with id: " + tabId + "."));
  },

  function closeTab() {
    chrome.tabs.create({url:"inspected.html"}, function(tab) {
      function onDetach(debuggee, reason) {
        chrome.test.assertEq(tab.id, debuggee.tabId);
        chrome.test.assertEq("target_closed", reason);
        chrome.debugger.onDetach.removeListener(onDetach);
        chrome.test.succeed();
      }

      var debuggee2 = {tabId: tab.id};
      chrome.debugger.attach(debuggee2, protocolVersion, function() {
        chrome.debugger.onDetach.addListener(onDetach);
        chrome.tabs.remove(tab.id);
      });
    });
  },

  function attachToWebUI() {
    chrome.tabs.create({url:"chrome://version"}, function(tab) {
      var debuggee = {tabId: tab.id};
      chrome.debugger.attach(debuggee, protocolVersion,
          fail("Can not attach to the page with the \"chrome://\" scheme."));
      chrome.tabs.remove(tab.id);
    });
  }

]);
