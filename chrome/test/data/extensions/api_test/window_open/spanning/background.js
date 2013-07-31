// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var nextTest = null;
openFromNormalShouldOpenInNormal();

function openFromNormalShouldOpenInNormal() {
  nextTest = openFromExtensionHostInIncognitoBrowserShouldOpenInNormalBrowser;
  chrome.windows.getAll({populate: true}, function(windows) {
    chrome.test.assertEq(1, windows.length);
    chrome.test.assertFalse(windows[0].incognito);
    chrome.test.assertEq(1, windows[0].tabs.length);
    chrome.test.assertFalse(windows[0].tabs[0].incognito);

    // The rest of the test continues in infobar.html.
    chrome.infobars.show({tabId: windows[0].tabs[0].id, path: "infobar.html"});
  });
}

function openFromExtensionHostInIncognitoBrowserShouldOpenInNormalBrowser() {
  nextTest = null;
  chrome.windows.getCurrent(function(normalWin) {
    chrome.test.assertFalse(normalWin.incognito);
    // Create an incognito window.
    chrome.windows.create({ incognito: true }, function(incognitoWin) {
      // Remove the normal window. We keep running because of the incognito
      // window.
      chrome.windows.remove(normalWin.id, function() {
        chrome.tabs.getAllInWindow(incognitoWin.id, function(tabs) {
          chrome.test.assertEq(1, tabs.length);
          // The rest of the test continues in infobar.html.
          chrome.infobars.show({tabId: tabs[0].id, path: "infobar.html"});
        });
      });
    });
  });
}

function verifyCreatedTab(tab) {
  // The new tab should be a normal tab, and it should be in a normal
  // window.
  chrome.test.assertFalse(tab.incognito);
  chrome.windows.get(tab.windowId, function(win) {
    chrome.test.assertFalse(win.incognito);
    if (nextTest) {
      nextTest();
    } else {
      chrome.test.notifyPass();
    }
  });
}
