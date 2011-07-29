// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the host resolver:
 *
 *   - Shows the default address family.
 *   - Has a button to enable IPv6, if it is disabled.
 *   - Shows the current host cache contents.
 *   - Has a button to clear the host cache.
 *   - Shows the parameters used to construct the host cache (capacity, ttl).
 *
 *  @constructor
 */
function DnsView() {
  const mainBoxId = 'dns-view-tab-content';
  const cacheTbodyId = 'dns-view-cache-tbody';
  const clearCacheButtonId = 'dns-view-clear-cache';
  const defaultFamilySpanId = 'dns-view-default-family';
  const ipv6DisabledSpanId = 'dns-view-ipv6-disabled';
  const enableIPv6ButtonId = 'dns-view-enable-ipv6';
  const capacitySpanId = 'dns-view-cache-capacity';
  const ttlSuccessSpanId = 'dns-view-cache-ttl-success';
  const ttlFailureSpanId = 'dns-view-cache-ttl-failure';

  DivView.call(this, mainBoxId);

  // Hook up the UI components.
  this.cacheTbody_ = $(cacheTbodyId);
  this.defaultFamilySpan_ = $(defaultFamilySpanId);
  this.ipv6DisabledSpan_ = $(ipv6DisabledSpanId);

  $(enableIPv6ButtonId).onclick = g_browser.enableIPv6.bind(g_browser);

  this.capacitySpan_ = $(capacitySpanId);
  this.ttlSuccessSpan_ = $(ttlSuccessSpanId);
  this.ttlFailureSpan_ = $(ttlFailureSpanId);

  var clearCacheButton = $(clearCacheButtonId);
  clearCacheButton.onclick =
      g_browser.sendClearHostResolverCache.bind(g_browser);

  // Register to receive changes to the host resolver info.
  g_browser.addHostResolverInfoObserver(this);
}

inherits(DnsView, DivView);

DnsView.prototype.onLoadLogFinish = function(data) {
  return this.onHostResolverInfoChanged(data.hostResolverInfo);
};

DnsView.prototype.onHostResolverInfoChanged = function(hostResolverInfo) {
  // Clear the existing values.
  this.defaultFamilySpan_.innerHTML = '';
  this.capacitySpan_.innerHTML = '';
  this.ttlSuccessSpan_.innerHTML = '';
  this.ttlFailureSpan_.innerHTML = '';
  this.cacheTbody_.innerHTML = '';

  // No info.
  if (!hostResolverInfo || !hostResolverInfo.cache)
    return false;

  var family = hostResolverInfo.default_address_family;
  addTextNode(this.defaultFamilySpan_, getKeyWithValue(AddressFamily, family));

  var ipv6Disabled = (family == AddressFamily.ADDRESS_FAMILY_IPV4);
  setNodeDisplay(this.ipv6DisabledSpan_, ipv6Disabled);

  // Fill in the basic cache information.
  var hostResolverCache = hostResolverInfo.cache;
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
    addTextNode(familyCell, getKeyWithValue(AddressFamily, e.address_family));

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

    var expiresDate = convertTimeTicksToDate(e.expiration);
    var expiresCell = addNode(tr, 'td');
    addTextNode(expiresCell, expiresDate.toLocaleString());
  }

  return true;
};
