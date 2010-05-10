// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the HTTP cache.
 *  @constructor
 */
function HttpCacheView(mainBoxId, reloadButtonId, statsDivId,
                       entryListingDivId) {
  DivView.call(this, mainBoxId);

  var reloadButton = document.getElementById(reloadButtonId);
  reloadButton.onclick = this.reloadListing_.bind(this);

  this.statsDiv_ = document.getElementById(statsDivId);
  this.entryListingDiv_ = document.getElementById(entryListingDivId);

  // Register to receive http cache info.
  g_browser.addHttpCacheInfoObserver(this);
}

inherits(HttpCacheView, DivView);

HttpCacheView.prototype.onHttpCacheInfoReceived = function(info) {
  this.entryListingDiv_.innerHTML = '';
  this.statsDiv_.innerHTML = '';

  // Print the statistics.
  var statsUl = addNode(this.statsDiv_, 'ul');
  for (var statName in info.stats) {
    var li = addNode(statsUl, 'li');
    addTextNode(li, statName + ': ' + info.stats[statName]);
  }

  // Print the entries.
  var keysOl = addNode(this.entryListingDiv_, 'ol');
  for (var i = 0; i < info.keys.length; ++i) {
    var key = info.keys[i];
    var li = addNode(keysOl, 'li');
    var a = addNode(li, 'a');
    addTextNode(a, key);
    a.href = 'chrome://view-http-cache/' + key;
    a.target = '_blank';
  }
};

HttpCacheView.prototype.reloadListing_ = function() {
  this.entryListingDiv_.innerHTML = 'Loading...';
  this.statsDiv_.innerHTML = 'Loading...';
  g_browser.sendGetHttpCacheInfo();
};
