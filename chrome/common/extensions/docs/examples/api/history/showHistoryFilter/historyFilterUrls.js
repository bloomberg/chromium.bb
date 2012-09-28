// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Event listner for clicks on links in a browser action popup.
// Open the link in a new tab of the current window.
function onAnchorClick(event) {
  chrome.tabs.create({
    selected: true,
    url: event.srcElement.href
  });
  return false;
}

// Given an array of URLs, build a DOM list of those URLs in the
// browser action popup.
function buildPopupDom(divName, data) {
  var popupDiv = document.getElementById(divName);

  var ul = document.createElement('ul');
  popupDiv.appendChild(ul);

  for (var i = 0, ie = data.length; i < ie; ++i) {
    var a = document.createElement('a');
    a.href = data[i].url;
    a.appendChild(document.createTextNode(data[i].title));
    a.addEventListener('click', onAnchorClick);

    var li = document.createElement('li');
    li.appendChild(a);

    ul.appendChild(li);
  }
}

// Search history to find up to ten links that a user has visited during the
// current time of the day +/- 1 hour.
function buildTypedUrlList(divName) {
  var millisecondsPerHour = 1000 * 60 * 60;
  var maxResults = 10;

  chrome.experimental.history.getMostVisited({
      'filterWidth' : millisecondsPerHour,
      'maxResults' : maxResults
    },
    function(historyItems) {
      buildPopupDom(divName, historyItems);
    }
  );
}

document.addEventListener('DOMContentLoaded', function () {
  buildTypedUrlList("filteredUrl_div");
});