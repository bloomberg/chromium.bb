// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

// TODO(mmenke):  Add links for each address entry to the corresponding NetLog
//                source.  This could either be done by adding NetLog source ids
//                to cache entries, or tracking sources based on their type and
//                description.  Former is simpler, latter may be useful
//                elsewhere as well.
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
    $(DnsView.IPV6_PROBE_BUTTON_ID).onclick = this.runIPv6Probe_.bind(this);
    $(DnsView.CLEAR_CACHE_BUTTON_ID).onclick =
        g_browser.sendClearHostResolverCache.bind(g_browser);

    // Used to track IPv6 probes.
    EventsTracker.getInstance().addLogEntryObserver(this);
    // ID of most recently started IPv6 probe job.  Once the job completes,
    // set back to -1.
    this.ipv6ProbeJobSourceId_ = -1;

    // Register to receive changes to the host resolver info.
    g_browser.addHostResolverInfoObserver(this, false);
  }

  DnsView.TAB_ID = 'tab-handle-dns';
  DnsView.TAB_NAME = 'DNS';
  DnsView.TAB_HASH = '#dns';

  // IDs for special HTML elements in dns_view.html
  DnsView.MAIN_BOX_ID = 'dns-view-tab-content';
  DnsView.DEFAULT_FAMILY_SPAN_ID = 'dns-view-default-family';
  DnsView.IPV6_DISABLED_SPAN_ID = 'dns-view-ipv6-disabled';
  DnsView.ENABLE_IPV6_BUTTON_ID = 'dns-view-enable-ipv6';

  DnsView.IPV6_PROBE_RUNNING_SPAN_ID = 'dns-view-ipv6-probe-running';
  DnsView.IPV6_PROBE_COMPLETE_SPAN_ID = 'dns-view-ipv6-probe-complete';
  DnsView.IPV6_PROBE_BUTTON_ID = 'dns-view-run-ipv6-probe';

  DnsView.INTERNAL_DNS_ENABLED_SPAN_ID = 'dns-view-internal-dns-enabled';
  DnsView.INTERNAL_DNS_INVALID_CONFIG_SPAN_ID =
      'dns-view-internal-dns-invalid-config';
  DnsView.INTERNAL_DNS_CONFIG_TBODY_ID = 'dns-view-internal-dns-config-tbody';

  DnsView.CLEAR_CACHE_BUTTON_ID = 'dns-view-clear-cache';
  DnsView.CAPACITY_SPAN_ID = 'dns-view-cache-capacity';

  DnsView.ACTIVE_SPAN_ID = 'dns-view-cache-active';
  DnsView.EXPIRED_SPAN_ID = 'dns-view-cache-expired';
  DnsView.CACHE_TBODY_ID = 'dns-view-cache-tbody';

  cr.addSingletonGetter(DnsView);

  DnsView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogStart: function(polledData, tabData, logDump) {
      // Clear information on whether or not an IPv6 probe is running.  Needs
      // to be done before loading the events.
      this.setIPv6ProbeJobLookupRunning_(false, -1);
    },

    onLoadLogFinish: function(data) {
      return this.onHostResolverInfoChanged(data.hostResolverInfo);
    },

    onHostResolverInfoChanged: function(hostResolverInfo) {
      // Clear the existing values.
      $(DnsView.DEFAULT_FAMILY_SPAN_ID).innerHTML = '';
      $(DnsView.CAPACITY_SPAN_ID).innerHTML = '';
      $(DnsView.CACHE_TBODY_ID).innerHTML = '';
      $(DnsView.ACTIVE_SPAN_ID).innerHTML = '0';
      $(DnsView.EXPIRED_SPAN_ID).innerHTML = '0';

      // Update fields containing async DNS configuration information.
      displayAsyncDnsConfig_(hostResolverInfo);

      // No info.
      if (!hostResolverInfo || !hostResolverInfo.cache)
        return false;

      var family = hostResolverInfo.default_address_family;
      addTextNode($(DnsView.DEFAULT_FAMILY_SPAN_ID),
                  addressFamilyToString(family));

      var ipv6Disabled = (family == AddressFamily.ADDRESS_FAMILY_IPV4);
      setNodeDisplay($(DnsView.IPV6_DISABLED_SPAN_ID), ipv6Disabled);

      // Fill in the basic cache information.
      var hostResolverCache = hostResolverInfo.cache;
      $(DnsView.CAPACITY_SPAN_ID).innerText = hostResolverCache.capacity;

      var expiredEntries = 0;
      // Date the cache was logged.  This will be either now, when actively
      // logging data, or the date the log dump was created.
      var logDate;
      if (MainView.isViewingLoadedLog()) {
        logDate = new Date(ClientInfo.numericDate);
      } else {
        logDate = new Date();
      }

      // Fill in the cache contents table.
      for (var i = 0; i < hostResolverCache.entries.length; ++i) {
        var e = hostResolverCache.entries[i];
        var tr = addNode($(DnsView.CACHE_TBODY_ID), 'tr');

        var hostnameCell = addNode(tr, 'td');
        addTextNode(hostnameCell, e.hostname);

        var familyCell = addNode(tr, 'td');
        addTextNode(familyCell,
                    addressFamilyToString(e.address_family));

        var addressesCell = addNode(tr, 'td');

        if (e.error != undefined) {
          var errorText =
              e.error + ' (' + netErrorToString(e.error) + ')';
          var errorNode = addTextNode(addressesCell, 'error: ' + errorText);
          addressesCell.classList.add('warning-text');
        } else {
          addListToNode_(addNode(addressesCell, 'div'), e.addresses);
        }

        var expiresDate = timeutil.convertTimeTicksToDate(e.expiration);
        var expiresCell = addNode(tr, 'td');
        timeutil.addNodeWithDate(expiresCell, expiresDate);
        if (logDate > timeutil.convertTimeTicksToDate(e.expiration)) {
          ++expiredEntries;
          var expiredSpan = addNode(expiresCell, 'span');
          expiredSpan.classList.add('warning-text');
          addTextNode(expiredSpan, ' [Expired]');
        }
      }

      $(DnsView.ACTIVE_SPAN_ID).innerText =
          hostResolverCache.entries.length - expiredEntries;
      $(DnsView.EXPIRED_SPAN_ID).innerText = expiredEntries;
      return true;
    },

    /**
     * Must be called whenever an IPv6 probe job starts or stops running.
     * @param {bool} running True if a probe job is running.
     * @param {sourceId} sourceId Source ID of the running probe job, if there
     *     is one.  -1 if |running| is false or we don't yet have the ID.
     */
    setIPv6ProbeJobLookupRunning_: function(running, sourceId) {
      setNodeDisplay($(DnsView.IPV6_PROBE_RUNNING_SPAN_ID), running);
      setNodeDisplay($(DnsView.IPV6_PROBE_COMPLETE_SPAN_ID), !running);
      this.ipv6ProbeJobSourceId_ = sourceId;
    },

    /**
     * Triggers a new IPv6 probe and displays the probe running message.
     */
    runIPv6Probe_: function() {
      // Since there's no source ID yet, have to just use -1.  We'll get the
      // ID when we see the start event for the probe.
      this.setIPv6ProbeJobLookupRunning_(true, -1);
      g_browser.sendRunIPv6Probe();
    },

    onReceivedLogEntries: function(logEntries) {
      for (var i = 0; i < logEntries.length; ++i) {
        if (logEntries[i].source.type != EventSourceType.IPV6_PROBE_JOB ||
            logEntries[i].type != EventType.IPV6_PROBE_RUNNING) {
          continue;
        }

        // For IPV6_PROBE_JOB events, update the display depending on whether or
        // not a probe job is running.  Only track the most recently started
        // probe job, as it will cancel any older jobs.
        if (logEntries[i].phase == EventPhase.PHASE_BEGIN) {
          this.setIPv6ProbeJobLookupRunning_(true, logEntries[i].source.id);
        } else if (logEntries[i].source.id == this.ipv6ProbeJobSourceId_) {
          this.setIPv6ProbeJobLookupRunning_(false, -1);
          g_browser.sendGetHostResolverInfo();
        }
      }
    },

    /**
     * Since the only thing that matters is the source ID of the active probe
     * job, which clearing events doesn't change, do nothing.
     */
    onAllLogEntriesDeleted: function() {
    },
  };

  /**
   * Displays information corresponding to the current async DNS configuration.
   * @param {Object} hostResolverInfo The host resolver information.
   */
  function displayAsyncDnsConfig_(hostResolverInfo) {
    // Clear the table.
    $(DnsView.INTERNAL_DNS_CONFIG_TBODY_ID).innerHTML = '';

    // Figure out if the internal DNS resolver is disabled or has no valid
    // configuration information, and update display accordingly.
    var enabled = hostResolverInfo &&
                  hostResolverInfo.dns_config !== undefined;
    var noConfig = enabled &&
                   hostResolverInfo.dns_config.nameservers === undefined;
    $(DnsView.INTERNAL_DNS_ENABLED_SPAN_ID).innerText = enabled;
    setNodeDisplay($(DnsView.INTERNAL_DNS_INVALID_CONFIG_SPAN_ID), noConfig);

    // If the internal DNS resolver is disabled or has no valid configuration,
    // we're done.
    if (!enabled || noConfig)
      return;

    var dnsConfig = hostResolverInfo.dns_config;

    // Display nameservers first.
    var nameserverRow = addNode($(DnsView.INTERNAL_DNS_CONFIG_TBODY_ID), 'tr');
    addNodeWithText(nameserverRow, 'th', 'nameservers');
    addListToNode_(addNode(nameserverRow, 'td'), dnsConfig.nameservers);

    // Add everything else in |dnsConfig| to the table.
    for (var key in dnsConfig) {
      if (key == 'nameservers')
        continue;
      var tr = addNode($(DnsView.INTERNAL_DNS_CONFIG_TBODY_ID), 'tr');
      addNodeWithText(tr, 'th', key);
      var td = addNode(tr, 'td');

      // For lists, display each list entry on a separate line.
      if (typeof dnsConfig[key] == 'object' &&
          dnsConfig[key].constructor == Array) {
        addListToNode_(td, dnsConfig[key]);
        continue;
      }

      addTextNode(td, dnsConfig[key]);
    }
  }

  /**
   * Takes a last of strings and adds them all to a DOM node, displaying them
   * on separate lines.
   * @param {DomNode} node The parent node.
   * @param {Array.<string>} list List of strings to add to the node.
   */
  function addListToNode_(node, list) {
    for (var i = 0; i < list.length; ++i)
      addNodeWithText(node, 'div', list[i]);
  }

  return DnsView;
})();
