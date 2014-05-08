// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

var tree = null;

function createTab(url, callback) {
  chrome.tabs.create({"url": url}, function(tab) {
      chrome.runtime.onMessage.addListener(
        function listener(message, sender) {
          if (!sender.tab)
            return;
          assertEq(tab.id, sender.tab.id);
          assertTrue(message['loaded']);
          callback();
          chrome.runtime.onMessage.removeListener(listener);
        });
      });
}

function setUpAndRunTests(allTests) {
  chrome.test.getConfig(function(config) {
    assertTrue('testServer' in config, 'Expected testServer in config');
    var url = "http://a.com:PORT/index.html"
        .replace(/PORT/, config.testServer.port);
    function gotTree(returnedTree) {
      tree = returnedTree;
      chrome.test.runTests(allTests);
    }
    createTab(url, function() {
      chrome.automation.getTree(gotTree);
    });
  });
}

