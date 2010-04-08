// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the host resolver:
 *
 *   - Shows the current host cache contents.
 *   - Has a button to clear the host cache.
 *   - Shows the parameters used to construct the host cache (capacity, ttl).
 *
 *  @constructor
 */
function DnsView(mainBoxId,
                 cacheTbodyId,
                 clearCacheButtonId,
                 capacitySpanId,
                 ttlSuccessSpanId,
                 ttlFailureSpanId) {
  DivView.call(this, mainBoxId);

  // Hook up the UI components.
  this.cacheTbody_ = document.getElementById(cacheTbodyId);
  this.capacitySpan_ = document.getElementById(capacitySpanId);
  this.ttlSuccessSpan_ = document.getElementById(ttlSuccessSpanId);
  this.ttlFailureSpan_ = document.getElementById(ttlFailureSpanId);

  var clearCacheButton = document.getElementById(clearCacheButtonId);
  clearCacheButton.onclick =
      g_browser.sendClearHostResolverCache.bind(g_browser);

  // Register to receive changes to the host resolver cache.
  g_browser.addHostResolverCacheObserver(this);
}

inherits(DnsView, DivView);

DnsView.prototype.onHostResolverCacheChanged = function(hostResolverCache) {
  // Clear the existing values.
  this.capacitySpan_.innerHTML = '';
  this.ttlSuccessSpan_.innerHTML = '';
  this.ttlFailureSpan_.innerHTML = '';
  this.cacheTbody_.innerHTML = '';

  // No cache.
  if (!hostResolverCache)
    return;

  // Fill in the basic cache information.
  addTextNode(this.capacitySpan_, hostResolverCache.capacity);
  addTextNode(this.ttlSuccessSpan_, hostResolverCache.ttl_success_ms);
  addTextNode(this.ttlFailureSpan_, hostResolverCache.ttl_failure_ms);

  // Fill in the cache contents table.
  for (var i = 0; i < hostResolverCache.entries.length; ++i) {
    var e = hostResolverCache.entries[i];
    var tr = addNode(this.cacheTbody_, 'tr');

    var hostnameCell = addNode(tr, 'td');
    addTextNode(hostnameCell, e.hostname);

    var familyCell = addNode(tr, 'td');
    addTextNode(familyCell, e.address_family);

    var addressesCell = addNode(tr, 'td');

    if (e.error != undefined) {
      addTextNode(addressesCell, 'error: ' + e.error);
    } else {
      for (var j = 0; j < e.addresses.length; ++j) {
        var address = e.addresses[j];
        if (j != 0)
          addNode(addressesCell, 'br');
        addTextNode(addressesCell, address);
      }
    }

    var expiresDate = g_browser.convertTimeTicksToDate(e.expiration);
    var expiresCell = addNode(tr, 'td');
    addTextNode(expiresCell, expiresDate.toLocaleString());
  }
};
