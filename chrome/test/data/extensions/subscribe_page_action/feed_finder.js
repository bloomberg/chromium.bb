// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// First check to see if this document is a feed. If so, it will redirect.
// Otherwise, check if it has embedded feed links, such as:
// (<link rel="alternate" type="application/rss+xml" etc). If so, show the
// page action icon.

if (!isPlainTextFeedDocument())
  findFeedLinks();

// See if the document contains a <link> tag within the <head> and
// whether that points to an RSS feed.
function findFeedLinks() {
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
    // Notify the extension needs to show the RSS page action icon.
    chrome.extension.sendRequest({msg: "feedIcon", feeds: feeds});
  }
}

// Check to see if the current document is a feed delivered as plain text,
// which Chrome does for some mime types.
function isPlainTextFeedDocument() {
  var body = document.body;

  // Chrome renders some content types like application/rss+xml and
  // application/atom+xml as text/plain, resulting in a body tag with one
  // PRE child containing the XML. So, we attempt to parse it as XML and look
  // for RSS tags within.
  if (body && body.childElementCount == 1 &&
      body.children[0].tagName == "PRE") {
    var domParser = new DOMParser();
    var doc = domParser.parseFromString(body.textContent, "text/xml");

    // Uncomment these three lines to see the parsing error.
    // var error = docWithin.getElementsByTagName("parsererror");
    // if (error.length)
    //  console.log('error: ' + doc.childNodes[0].outerHTML);

    // |doc| now contains the parsed document within the PRE tag.
    if (containsFeed(doc)) {
      // Let the extension know that we should show the subscribe page.
      chrome.extension.sendRequest({msg: "feedDocument", href: location.href});
      return true;
    }
  }

  return false;
}
