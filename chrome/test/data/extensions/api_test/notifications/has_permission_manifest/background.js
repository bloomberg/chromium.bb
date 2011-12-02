// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var notification = null;
var chromeExtensionsUrl = "chrome://extensions/";

// Shows the notification window using the specified URL.
// Control continues at onNotificationDone().
function showNotification(url) {
  notification = window.webkitNotifications.createHTMLNotification(url);
  notification.onerror = function() {
    chrome.test.fail("Failed to show notification.");
  };
  notification.show();
}

// Called by the notification when it is done with its tests.
function onNotificationDone() {
  var views = chrome.extension.getViews();
  chrome.test.assertEq(2, views.length);
  notification.cancel();

  // This last step tests that crbug.com/40967 stays fixed.
  var listener = function(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete')
      return;
    // web_page1 loaded, open extension page to inject script
    if (tab.url == chromeExtensionsUrl) {
      console.log(chromeExtensionsUrl + ' finished loading.');
      chrome.tabs.onUpdated.removeListener(listener);
      chrome.test.succeed();
    }
  };

  chrome.tabs.onUpdated.addListener(listener);
  chrome.tabs.create({ url: chromeExtensionsUrl });
}

chrome.test.runTests([
  function hasPermission() {
    chrome.test.assertEq(0,  // allowed
                         webkitNotifications.checkPermission());
    chrome.test.succeed();
  },
  function absoluteURL() {
    showNotification(chrome.extension.getURL("notification.html"));
  },
  function relativeURL() {
    showNotification('notification.html');
  }
]);
