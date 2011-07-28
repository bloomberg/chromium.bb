// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the HTTP cache.
 *  @constructor
 */
function HttpCacheView() {
  const mainBoxId = 'httpCacheTabContent';
  const statsDivId = 'httpCacheStats';

  DivView.call(this, mainBoxId);

  this.statsDiv_ = $(statsDivId);

  // Register to receive http cache info.
  g_browser.addHttpCacheInfoObserver(this);
}

inherits(HttpCacheView, DivView);

HttpCacheView.prototype.onLoadLogFinish = function(data) {
  return this.onHttpCacheInfoChanged(data.httpCacheInfo);
};

HttpCacheView.prototype.onHttpCacheInfoChanged = function(info) {
  this.statsDiv_.innerHTML = '';

  if (!info)
    return false;

  // Print the statistics.
  var statsUl = addNode(this.statsDiv_, 'ul');
  for (var statName in info.stats) {
    var li = addNode(statsUl, 'li');
    addTextNode(li, statName + ': ' + info.stats[statName]);
  }
  return true;
};

