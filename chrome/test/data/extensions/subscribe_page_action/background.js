// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A dictionary keyed off of tabId that keeps track of data per tab (for
// example what feedUrl was detected in the tab).
var feedData = {};

chrome.extension.onRequest.addListener(function(request, sender) {
  if (request.msg == "feedIcon") {
    // First validate that all the URLs have the right schema.
    var input = [];
    for (var i = 0; i < request.feeds.length; ++i) {
      var a = document.createElement('a');
      a.href = request.feeds[i].href;
      if (a.protocol == "http:" || a.protocol == "https:") {
        input.push(request.feeds[i]);
      } else {
        console.log('Warning: feed source rejected (wrong protocol): ' +
                    request.feeds[i].href);
      }
    }

    if (input.length == 0)
      return;  // We've rejected all the input, so abort.

    // We have received a list of feed urls found on the page.
    // Enable the page action icon.
    feedData[sender.tab.id] = input;
    chrome.pageAction.setTitle(
      { tabId: sender.tab.id,
        title: chrome.i18n.getMessage("rss_subscription_action_title")
      });
    chrome.pageAction.show(sender.tab.id);
  } else if (request.msg == "feedDocument") {
    // We received word from the content script that this document
    // is an RSS feed (not just a document linking to the feed).
    // So, we go straight to the subscribe page in a new tab and
    // navigate back on the current page (to get out of the xml page).
    // We don't want to navigate in-place because trying to go back
    // from the subscribe page takes us back to the xml page, which
    // will redirect to the subscribe page again (we don't support a
    // location.replace equivalant in the Tab navigation system).
    chrome.tabs.executeScript(sender.tab.id,
        { code: "if (history.length > 1) " +
                 "history.go(-1); else window.close();"
        });
    var url = "subscribe.html?" + encodeURIComponent(request.href);
    url = chrome.extension.getURL(url);
    chrome.tabs.create({ url: url, index: sender.tab.index });
  }
});

chrome.tabs.onRemoved.addListener(function(tabId) {
  delete feedData[tabId];
});
