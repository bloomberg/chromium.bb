// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays options for importing/exporting the captured data. Its
 * primarily usefulness is to allow users to copy-paste their data in an easy
 * to read format for bug reports.
 *
 *   - Has a button to generate a text report.
 *
 *   - Shows how many events have been captured.
 *  @constructor
 */
function DataView(mainBoxId,
                  outputTextBoxId,
                  exportTextButtonId,
                  securityStrippingCheckboxId,
                  byteLoggingCheckboxId,
                  passivelyCapturedCountId,
                  activelyCapturedCountId,
                  deleteAllId,
                  dumpDataDivId,
                  loadDataDivId,
                  loadLogFileId,
                  capturingTextSpanId,
                  loggingTextSpanId) {
  DivView.call(this, mainBoxId);

  this.textPre_ = document.getElementById(outputTextBoxId);

  var securityStrippingCheckbox =
      document.getElementById(securityStrippingCheckboxId);
  securityStrippingCheckbox.onclick =
      this.onSetSecurityStripping_.bind(this, securityStrippingCheckbox);

  var byteLoggingCheckbox = document.getElementById(byteLoggingCheckboxId);
  byteLoggingCheckbox.onclick =
      this.onSetByteLogging_.bind(this, byteLoggingCheckbox);

  var exportTextButton = document.getElementById(exportTextButtonId);
  exportTextButton.onclick = this.onExportToText_.bind(this);

  this.activelyCapturedCountBox_ =
      document.getElementById(activelyCapturedCountId);
  this.passivelyCapturedCountBox_ =
      document.getElementById(passivelyCapturedCountId);
  document.getElementById(deleteAllId).onclick =
      g_browser.deleteAllEvents.bind(g_browser);

  this.dumpDataDiv_ = document.getElementById(dumpDataDivId);
  this.loadDataDiv_ = document.getElementById(loadDataDivId);
  this.capturingTextSpan_ = document.getElementById(capturingTextSpanId);
  this.loggingTextSpan_ = document.getElementById(loggingTextSpanId);

  document.getElementById(loadLogFileId).onclick =
      g_browser.loadLogFile.bind(g_browser);

  this.updateEventCounts_();
  this.waitingForUpdate_ = false;

  g_browser.addLogObserver(this);
}

inherits(DataView, DivView);

/**
 * Called whenever a new event is received.
 */
DataView.prototype.onLogEntryAdded = function(logEntry) {
  this.updateEventCounts_();
};

/**
 * Called whenever some log events are deleted.  |sourceIds| lists
 * the source IDs of all deleted log entries.
 */
DataView.prototype.onLogEntriesDeleted = function(sourceIds) {
  this.updateEventCounts_();
};

/**
 * Called whenever all log events are deleted.
 */
DataView.prototype.onAllLogEntriesDeleted = function() {
  this.updateEventCounts_();
};

/**
 * Called when either a log file is loaded or when going back to actively
 * logging events.  In either case, called after clearing the old entries,
 * but before getting any new ones.
 */
DataView.prototype.onSetIsViewingLogFile = function(isViewingLogFile) {
  setNodeDisplay(this.dumpDataDiv_, !isViewingLogFile);
  setNodeDisplay(this.capturingTextSpan_, !isViewingLogFile);
  setNodeDisplay(this.loggingTextSpan_, isViewingLogFile);
  this.setText_('');
};

/**
 * Updates the counters showing how many events have been captured.
 */
DataView.prototype.updateEventCounts_ = function() {
  this.activelyCapturedCountBox_.innerText =
      g_browser.getNumActivelyCapturedEvents()
  this.passivelyCapturedCountBox_.innerText =
      g_browser.getNumPassivelyCapturedEvents();
};

/**
 * Depending on the value of the checkbox, enables or disables logging of
 * actual bytes transferred.
 */
DataView.prototype.onSetByteLogging_ = function(byteLoggingCheckbox) {
  if (byteLoggingCheckbox.checked) {
    g_browser.setLogLevel(LogLevelType.LOG_ALL);
  } else {
    g_browser.setLogLevel(LogLevelType.LOG_ALL_BUT_BYTES);
  }
};

/**
 * Depending on the value of the checkbox, enables or disables stripping
 * cookies and passwords from log dumps and displayed events.
 */
DataView.prototype.onSetSecurityStripping_ =
    function(securityStrippingCheckbox) {
  g_browser.setSecurityStripping(securityStrippingCheckbox.checked);
};

/**
 * Clears displayed text when security stripping is toggled.
 */
DataView.prototype.onSecurityStrippingChanged = function() {
  this.setText_('');
}

/**
 * If not already waiting for results from all updates, triggers all
 * updates and starts waiting for them to complete.
 */
DataView.prototype.onExportToText_ = function() {
  if (this.waitingForUpdate_)
    return;
  this.waitingForUpdate = true;
  this.setText_('Generating...');
  g_browser.updateAllInfo(this.onUpdateAllCompleted.bind(this));
};

/**
 * Presents the captured data as formatted text.
 */
DataView.prototype.onUpdateAllCompleted = function(data) {
  // It's possible for a log file to be loaded while a dump is being generated.
  // When that happens, don't display the log dump, to avoid any confusion.
  if (g_browser.isViewingLogFile())
    return;
  this.waitingForUpdate_ = false;
  var text = [];

  // Print some basic information about our environment.
  text.push('Data exported on: ' + (new Date()).toLocaleString());
  text.push('');
  text.push('Number of passively captured events: ' +
            g_browser.getNumPassivelyCapturedEvents());
  text.push('Number of actively captured events: ' +
            g_browser.getNumActivelyCapturedEvents());
  text.push('');

  text.push('Chrome version: ' + ClientInfo.version +
            ' (' + ClientInfo.official +
            ' ' + ClientInfo.cl +
            ') ' + ClientInfo.version_mod);
  // Third value in first set of parentheses in user-agent string.
  var platform = /\(.*?;.*?; (.*?);/.exec(navigator.userAgent);
  if (platform)
    text.push('Platform: ' + platform[1]);
  text.push('Command line: ' + ClientInfo.command_line);

  text.push('');
  var default_address_family = data.hostResolverInfo.default_address_family;
  text.push('Default address family: ' +
      getKeyWithValue(AddressFamily, default_address_family));
  if (default_address_family == AddressFamily.ADDRESS_FAMILY_IPV4)
    text.push('  (IPv6 disabled)');

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Proxy settings (effective)');
  text.push('----------------------------------------------');
  text.push('');

  text.push(proxySettingsToString(data.proxySettings.effective));

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Proxy settings (original)');
  text.push('----------------------------------------------');
  text.push('');

  text.push(proxySettingsToString(data.proxySettings.original));

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Bad proxies cache');
  text.push('----------------------------------------------');

  var badProxiesList = data.badProxies;
  if (badProxiesList.length == 0) {
    text.push('');
    text.push('None');
  } else {
    for (var i = 0; i < badProxiesList.length; ++i) {
      var e = badProxiesList[i];
      text.push('');
      text.push('(' + (i+1) + ')');
      text.push('Proxy: ' + e.proxy_uri);
      text.push('Bad until: ' + this.formatExpirationTime_(e.bad_until));
    }
  }

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Host resolver cache');
  text.push('----------------------------------------------');
  text.push('');

  var hostResolverCache = data.hostResolverInfo.cache;

  text.push('Capacity: ' + hostResolverCache.capacity);
  text.push('Time to live for successful resolves (ms): ' +
            hostResolverCache.ttl_success_ms);
  text.push('Time to live for failed resolves (ms): ' +
            hostResolverCache.ttl_failure_ms);

  if (hostResolverCache.entries.length > 0) {
    for (var i = 0; i < hostResolverCache.entries.length; ++i) {
      var e = hostResolverCache.entries[i];

      text.push('');
      text.push('(' + (i+1) + ')');
      text.push('Hostname: ' + e.hostname);
      text.push('Address family: ' +
                getKeyWithValue(AddressFamily, e.address_family));

      if (e.error != undefined) {
         text.push('Error: ' + e.error);
      } else {
        for (var j = 0; j < e.addresses.length; ++j) {
          text.push('Address ' + (j + 1) + ': ' + e.addresses[j]);
        }
      }

      text.push('Valid until: ' + this.formatExpirationTime_(e.expiration));
      var expirationDate = g_browser.convertTimeTicksToDate(e.expiration);
      text.push('  (' + expirationDate.toLocaleString() + ')');
    }
  } else {
    text.push('');
    text.push('None');
  }

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Events');
  text.push('----------------------------------------------');
  text.push('');

  this.appendEventsPrintedAsText_(text);

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Http cache stats');
  text.push('----------------------------------------------');
  text.push('');

  var httpCacheStats = data.httpCacheInfo.stats;
  for (var statName in httpCacheStats)
    text.push(statName + ': ' + httpCacheStats[statName]);

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Socket pools');
  text.push('----------------------------------------------');
  text.push('');

  this.appendSocketPoolsAsText_(text, data.socketPoolInfo);

  text.push('');
  text.push('----------------------------------------------');
  text.push(' SPDY Status');
  text.push('----------------------------------------------');
  text.push('');

  text.push('SPDY Enabled: ' + data.spdyStatus.spdy_enabled);
  text.push('Use Alternate Protocol: ' +
      data.spdyStatus.use_alternate_protocols);
  text.push('Force SPDY Always: ' + data.spdyStatus.force_spdy_always);
  text.push('Force SPDY Over SSL: ' + data.spdyStatus.force_spdy_over_ssl);
  text.push('Next Protocols: ' + data.spdyStatus.next_protos);


  text.push('');
  text.push('----------------------------------------------');
  text.push(' SPDY Sessions');
  text.push('----------------------------------------------');
  text.push('');

  if (data.spdySessionInfo == null || data.spdySessionInfo.length == 0) {
    text.push('None');
  } else {
    var spdyTablePrinter =
      SpdyView.createSessionTablePrinter(data.spdySessionInfo);
    text.push(spdyTablePrinter.toText(2));
  }

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Alternate Protocol Mappings');
  text.push('----------------------------------------------');
  text.push('');

  if (data.spdyAlternateProtocolMappings == null ||
      data.spdyAlternateProtocolMappings.length == 0) {
    text.push('None');
  } else {
    var spdyTablePrinter =
      SpdyView.createAlternateProtocolMappingsTablePrinter(
          data.spdyAlternateProtocolMappings);
    text.push(spdyTablePrinter.toText(2));
  }

  if (g_browser.isPlatformWindows()) {
    text.push('');
    text.push('----------------------------------------------');
    text.push(' Winsock layered service providers');
    text.push('----------------------------------------------');
    text.push('');

    var serviceProviders = data.serviceProviders;
    var layeredServiceProviders = serviceProviders.service_providers;
    for (var i = 0; i < layeredServiceProviders.length; ++i) {
      var provider = layeredServiceProviders[i];
      text.push('name: ' + provider.name);
      text.push('version: ' + provider.version);
      text.push('type: ' +
                ServiceProvidersView.getLayeredServiceProviderType(provider));
      text.push('socket_type: ' +
                ServiceProvidersView.getSocketType(provider));
      text.push('socket_protocol: ' +
                ServiceProvidersView.getProtocolType(provider));
      text.push('path: ' + provider.path);
      text.push('');
    }

    text.push('');
    text.push('----------------------------------------------');
    text.push(' Winsock namespace providers');
    text.push('----------------------------------------------');
    text.push('');

    var namespaceProviders = serviceProviders.namespace_providers;
    for (var i = 0; i < namespaceProviders.length; ++i) {
      var provider = namespaceProviders[i];
      text.push('name: ' + provider.name);
      text.push('version: ' + provider.version);
      text.push('type: ' +
                ServiceProvidersView.getNamespaceProviderType(provider));
      text.push('active: ' + provider.active);
      text.push('');
    }
  }

  // Open a new window to display this text.
  this.setText_(text.join('\n'));

  this.selectText_();
};

DataView.prototype.appendEventsPrintedAsText_ = function(out) {
  var allEvents = g_browser.getAllCapturedEvents();

  // Group the events into buckets by source ID, and buckets by source type.
  var sourceIds = [];
  var sourceIdToEventList = {};
  var sourceTypeToSourceIdList = {};

  // Lists used for actual output.
  var eventLists = [];

  for (var i = 0; i < allEvents.length; ++i) {
    var e = allEvents[i];
    var eventList = sourceIdToEventList[e.source.id];
    if (!eventList) {
      eventList = [];
      eventLists.push(eventList);
      if (e.source.type != LogSourceType.NONE)
        sourceIdToEventList[e.source.id] = eventList;

      // Update sourceIds
      sourceIds.push(e.source.id);

      // Update the sourceTypeToSourceIdList list.
      var idList = sourceTypeToSourceIdList[e.source.type];
      if (!idList) {
        idList = [];
        sourceTypeToSourceIdList[e.source.type] = idList;
      }
      idList.push(e.source.id);
    }
    eventList.push(e);
  }


  // For each source or event without a source (ordered by when the first
  // output event for that source happened).
  for (var i = 0; i < eventLists.length; ++i) {
    var eventList = eventLists[i];
    var sourceId = eventList[0].source.id;
    var sourceType = eventList[0].source.type;

    var startDate = g_browser.convertTimeTicksToDate(eventList[0].time);

    out.push('------------------------------------------');
    out.push(getKeyWithValue(LogSourceType, sourceType) +
             ' (id=' + sourceId + ')' +
             '  [start=' + startDate.toLocaleString() + ']');
    out.push('------------------------------------------');

    out.push(PrintSourceEntriesAsText(eventList));
  }
};

DataView.prototype.appendSocketPoolsAsText_ = function(text, socketPoolInfo) {
  var socketPools = SocketPoolWrapper.createArrayFrom(socketPoolInfo);
  var tablePrinter = SocketPoolWrapper.createTablePrinter(socketPools);
  text.push(tablePrinter.toText(2));

  text.push('');

  for (var i = 0; i < socketPools.length; ++i) {
    if (socketPools[i].origPool.groups == undefined)
      continue;
    var groupTablePrinter = socketPools[i].createGroupTablePrinter();
    text.push(groupTablePrinter.toText(2));
  }
};

/**
 * Helper function to set this view's content to |text|.
 */
DataView.prototype.setText_ = function(text) {
  this.textPre_.innerHTML = '';
  addTextNode(this.textPre_, text);
};

/**
 * Format a time ticks count as a timestamp.
 */
DataView.prototype.formatExpirationTime_ = function(timeTicks) {
  var d = g_browser.convertTimeTicksToDate(timeTicks);
  var isExpired = d.getTime() < (new Date()).getTime();
  return 't=' + d.getTime() + (isExpired ? ' [EXPIRED]' : '');
};

/**
 * Select all text from log dump.
 */
DataView.prototype.selectText_ = function() {
  var selection = window.getSelection();
  selection.removeAllRanges();

  var range = document.createRange();
  range.selectNodeContents(this.textPre_);
  selection.addRange(range);
};
