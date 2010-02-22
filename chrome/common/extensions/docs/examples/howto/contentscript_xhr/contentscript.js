/*
 * Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
 * source code is governed by a BSD-style license that can be found in the
 * LICENSE file.
 */

/**
 * Parses text from Twitter's API and generates a bar with trending topics at
 * the top of the current page
 * @param data Object JSON decoded response.  Null if the request failed.
 */
function onText(data) {
  // Only render the bar if the data is parsed into a format we recognize.
  if (data.trends) {
    var trend_names = []
    for (var date in data.trends) {
      if (data.trends.hasOwnProperty(date)) {
        var trends = data.trends[date];
        for (var i=0,trend; trend = trends[i]; i++) {
          trend_names.push(trend.name);
        }
      }
    }

    // Create the overlay at the top of the page and fill it with data.
    var trends_dom = document.createElement('div');
    var title_dom = document.createElement('strong');
    var text_dom = document.createTextNode(trend_names.join(', '));
    title_dom.innerText = 'Topics currently trending on Twitter ';
    trends_dom.appendChild(title_dom);
    trends_dom.appendChild(text_dom);
    trends_dom.style.background = '#36b';
    trends_dom.style.color = '#fff';
    trends_dom.style.padding = '10px';
    trends_dom.style.position = 'relative';
    trends_dom.style.zIndex = '123456';
    trends_dom.style.font = '14px Arial';
    document.body.insertBefore(trends_dom, document.body.firstChild);
  }
};

// Send a request to fetch data from Twitter's API to the background page.
// Specify that onText should be called with the result.
chrome.extension.sendRequest({'action' : 'fetchTwitterFeed'}, onText);
