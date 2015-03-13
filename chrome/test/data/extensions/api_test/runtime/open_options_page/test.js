// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_tests --gtest_filter=ExtensionApiTest.OpenOptionsPage

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var listenOnce = chrome.test.listenOnce;
var pass = chrome.test.callbackPass;

var optionsTabUrl = 'chrome://extensions/?options=' + chrome.runtime.id;

// Finds the Tab for an options page, or null if no options page is open.
// Asserts that there is at most 1 options page open.
// Result is passed to |callback|.
function findOptionsTab(callback) {
  chrome.tabs.query({url: optionsTabUrl}, pass(function(tabs) {
    assertTrue(tabs.length <= 1);
    callback(tabs.length == 0 ? null : tabs[0]);
  }));
}

// Tests opening a new options page.
function testNewOptionsPage() {
  findOptionsTab(function(tab) {
    assertEq(null, tab);
    listenOnce(chrome.runtime.onMessage, function(m, sender) {
      assertEq('success', m);
      assertEq(chrome.runtime.id, sender.id);
      assertEq(chrome.runtime.getURL('options.html'), sender.url);
    });
    chrome.runtime.openOptionsPage();
  });
}

// Gets the active tab, or null if no tab is active. Asserts that there is at
// most 1 active tab. Result is passed to |callback|.
function getActiveTab(callback) {
  chrome.tabs.query({active: true}, pass(function(tabs) {
    assertTrue(tabs.length <= 1);
    callback(tabs.length == 0 ? null : tabs[0]);
  }));
}

// Tests refocusing an existing page.
function testRefocusExistingOptionsPage() {
  var testUrl = 'chrome://chrome/';

  // There will already be an options page open from the last test. Find it,
  // focus away from it, then make sure openOptionsPage() refocuses it.
  findOptionsTab(function(optionsTab) {
    assertTrue(optionsTab != null);
    chrome.tabs.create({url: testUrl}, pass(function(tab) {
      // Make sure the new tab is active.
      getActiveTab(function(activeTab) {
        assertEq(testUrl, activeTab.url);
        // Open options page should refocus it.
        chrome.runtime.openOptionsPage();
        getActiveTab(function(activeTab) {
          assertEq(optionsTabUrl, activeTab.url);
        });
      });
    }));
  });
}

chrome.test.runTests([testNewOptionsPage, testRefocusExistingOptionsPage]);
