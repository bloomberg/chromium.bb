// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays options for importing/exporting the captured data. Its
 * primarily usefulness is to allow users to copy-paste their data in an easy
 * to read format for bug reports.
 *
 *   - Has a button to generate a text report.
 *   - Has a button to generate a raw JSON dump (most useful for testing).
 *
 *  @constructor
 */
function DataView(mainBoxId, outputTextBoxId,
                  exportTextButtonId, stripCookiesCheckboxId) {
  DivView.call(this, mainBoxId);

  this.textPre_ = document.getElementById(outputTextBoxId);
  this.stripCookiesCheckbox_ = document.getElementById(stripCookiesCheckboxId);

  var exportTextButton = document.getElementById(exportTextButtonId);
  exportTextButton.onclick = this.onExportToText_.bind(this);
}

inherits(DataView, DivView);

/**
 * Presents the captured data as formatted text.
 */
DataView.prototype.onExportToText_ = function() {
  this.setText_("Generating...");

  var text = [];

  // Print some basic information about our environment.
  text.push('Data exported on: ' + (new Date()).toLocaleString());
  text.push('');
  text.push('Number of passively captured events: ' +
            g_browser.getAllPassivelyCapturedEvents().length);
  text.push('Number of actively captured events: ' +
            g_browser.getAllActivelyCapturedEvents().length);
  text.push('');

  text.push('Chrome version: ' + ClientInfo.version +
            ' (' + ClientInfo.official +
            ' ' + ClientInfo.cl +
            ') ' + ClientInfo.version_mod);
  text.push('Command line: ' + ClientInfo.command_line);

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Proxy settings (effective)');
  text.push('----------------------------------------------');
  text.push('');

  text.push(proxySettingsToString(
        g_browser.proxySettings_.currentData_.effective));

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Proxy settings (original)');
  text.push('----------------------------------------------');
  text.push('');

  text.push(proxySettingsToString(
        g_browser.proxySettings_.currentData_.original));

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Bad proxies cache');
  text.push('----------------------------------------------');

  var badProxiesList = g_browser.badProxies_.currentData_;
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

  var hostResolverCache = g_browser.hostResolverCache_.currentData_;

  text.push('Capcity: ' + hostResolverCache.capacity);
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
      text.push('Address family: ' + e.address_family);

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
  text.push(' Requests');
  text.push('----------------------------------------------');
  text.push('');

  this.appendRequestsPrintedAsText_(text);

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Http cache stats');
  text.push('----------------------------------------------');
  text.push('');

  var httpCacheStats = g_browser.httpCacheInfo_.currentData_.stats;
  for (var statName in httpCacheStats)
    text.push(statName + ': ' + httpCacheStats[statName]);

  // Open a new window to display this text.
  this.setText_(text.join('\n'));

  this.selectText_();
};

DataView.prototype.appendRequestsPrintedAsText_ = function(out) {
  // Concatenate the passively captured events with the actively captured events
  // into a single array.
  var allEvents = g_browser.getAllPassivelyCapturedEvents().concat(
      g_browser.getAllActivelyCapturedEvents());

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

    out.push(PrintSourceEntriesAsText(eventList,
                                      this.stripCookiesCheckbox_.checked));
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
