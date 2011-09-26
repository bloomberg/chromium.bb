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
 */

var DnsView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   *  @constructor
   */
  function DnsView() {
    assertFirstConstructorCall(DnsView);

    // Call superclass's constructor.
    superClass.call(this, DnsView.MAIN_BOX_ID);

    $(DnsView.ENABLE_IPV6_BUTTON_ID).onclick =
        g_browser.enableIPv6.bind(g_browser);
    $(DnsView.CLEAR_CACHE_BUTTON_ID).onclick =
        g_browser.sendClearHostResolverCache.bind(g_browser);

    // Register to receive changes to the host resolver info.
    g_browser.addHostResolverInfoObserver(this);
  }

  // ID for special HTML element in category_tabs.html
  DnsView.TAB_HANDLE_ID = 'tab-handle-dns';

  // IDs for special HTML elements in dns_view.html
  DnsView.MAIN_BOX_ID = 'dns-view-tab-content';
  DnsView.CACHE_TBODY_ID = 'dns-view-cache-tbody';
  DnsView.CLEAR_CACHE_BUTTON_ID = 'dns-view-clear-cache';
  DnsView.DEFAULT_FAMILY_SPAN_ID = 'dns-view-default-family';
  DnsView.IPV6_DISABLED_SPAN_ID = 'dns-view-ipv6-disabled';
  DnsView.ENABLE_IPV6_BUTTON_ID = 'dns-view-enable-ipv6';
  DnsView.CAPACITY_SPAN_ID = 'dns-view-cache-capacity';
  DnsView.TTL_SUCCESS_SPAN_ID = 'dns-view-cache-ttl-success';
  DnsView.TTL_FAILURE_SPAN_ID = 'dns-view-cache-ttl-failure';

  cr.addSingletonGetter(DnsView);

  DnsView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onHostResolverInfoChanged(data.hostResolverInfo);
    },

    onHostResolverInfoChanged: function(hostResolverInfo) {
      // Clear the existing values.
      $(DnsView.DEFAULT_FAMILY_SPAN_ID).innerHTML = '';
      $(DnsView.CAPACITY_SPAN_ID).innerHTML = '';
      $(DnsView.TTL_SUCCESS_SPAN_ID).innerHTML = '';
      $(DnsView.TTL_FAILURE_SPAN_ID).innerHTML = '';
      $(DnsView.CACHE_TBODY_ID).innerHTML = '';

      // No info.
      if (!hostResolverInfo || !hostResolverInfo.cache)
        return false;

      var family = hostResolverInfo.default_address_family;
      addTextNode($(DnsView.DEFAULT_FAMILY_SPAN_ID),
                  getKeyWithValue(AddressFamily, family));

      var ipv6Disabled = (family == AddressFamily.ADDRESS_FAMILY_IPV4);
      setNodeDisplay($(DnsView.IPV6_DISABLED_SPAN_ID), ipv6Disabled);

      // Fill in the basic cache information.
      var hostResolverCache = hostResolverInfo.cache;
      $(DnsView.CAPACITY_SPAN_ID).innerText = hostResolverCache.capacity;
      $(DnsView.TTL_SUCCESS_SPAN_ID).innerText =
          hostResolverCache.ttl_success_ms;
      $(DnsView.TTL_FAILURE_SPAN_ID).innerText =
          hostResolverCache.ttl_failure_ms;

      // Fill in the cache contents table.
      for (var i = 0; i < hostResolverCache.entries.length; ++i) {
        var e = hostResolverCache.entries[i];
        var tr = addNode($(DnsView.CACHE_TBODY_ID), 'tr');

        var hostnameCell = addNode(tr, 'td');
        addTextNode(hostnameCell, e.hostname);

        var familyCell = addNode(tr, 'td');
        addTextNode(familyCell,
                    getKeyWithValue(AddressFamily, e.address_family));

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

        var expiresDate = timeutil.convertTimeTicksToDate(e.expiration);
        var expiresCell = addNode(tr, 'td');
        addTextNode(expiresCell, expiresDate.toLocaleString());
      }

      return true;
    }
  };

  return DnsView;
})();
