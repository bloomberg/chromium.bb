// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

findFeeds();

function findFeeds() {
  // Find all the RSS link elements.
  var result = document.evaluate(
      '//*[local-name()="link"][@rel="alternate"][contains(@type, "rss") or ' +
      'contains(@type, "atom") or contains(@type, "rdf")]',
      document, null, 0, null);

  var feeds = [];
  var item;
  var count = 0;
  while (item = result.iterateNext()) {
    feeds.push({"href": item.href, "title": item.title});
    ++count;
  }

  if (count > 0) {
    // Notify the extension of the feed URLs we found.
    chrome.extension.sendRequest({msg: "feedIcon", feeds: feeds});
  }
}
