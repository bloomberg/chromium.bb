// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

var debuggee;
var protocolVersion = "1.0";

chrome.test.runTests([

  function attach() {
    var extensionId = chrome.extension.getURL('').split('/')[2];
    debuggee = {extensionId: extensionId};
    chrome.debugger.attach(debuggee, protocolVersion, pass());
  },

  function attachToMissing() {
    var missingDebuggee = {extensionId: "foo"};
    chrome.debugger.attach(missingDebuggee, protocolVersion,
        fail("No background page with given id " +
            missingDebuggee.extensionId + "."));
  },

  function attachAgain() {
    chrome.debugger.attach(debuggee, protocolVersion,
        fail("Another debugger is already attached " +
            "to the background page with id: " + debuggee.extensionId + "."));
  },

  function detach() {
    chrome.debugger.detach(debuggee, pass());
  },

  function detachAgain() {
    chrome.debugger.detach(debuggee,
        fail("Debugger is not attached to the background page with id: " +
            debuggee.extensionId + "."));
  },

  function discoverOwnBackgroundPage() {
    chrome.debugger.getTargets(function(targets) {
      var target = targets.filter(
          function(t) {
            return t.type == 'background_page' &&
                t.title == 'Extension Debugger';
          })[0];
      if (target) {
        chrome.debugger.attach({targetId: target.id}, protocolVersion, pass());
      } else {
        chrome.test.fail("No extension found");
      }
    });
  }
]);
