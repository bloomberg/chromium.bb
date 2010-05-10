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
function DataView(mainBoxId, outputTextBoxId, exportTextButtonId) {
  DivView.call(this, mainBoxId);

  this.textPre_ = document.getElementById(outputTextBoxId);
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
  text.push('Timetick to timestamp offset: ' + g_browser.timeTickOffset_);
  text.push('');
  // TODO(eroman): fill this with proper values.
  text.push('Chrome version: ' + 'TODO');
  text.push('Command line switches: ' + 'TODO');

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Proxy settings');
  text.push('----------------------------------------------');
  text.push('');

  text.push(g_browser.proxySettings_.currentData_);

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Bad proxies cache');
  text.push('----------------------------------------------');
  text.push('');

  var badProxiesList = g_browser.badProxies_.currentData_;
  if (badProxiesList.length == 0) {
    text.push('None');
  } else {
    this.appendPrettyPrintedTable_(badProxiesList, text);
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
    text.push('');
    this.appendPrettyPrintedTable_(hostResolverCache.entries, text);
  }

  text.push('');
  text.push('----------------------------------------------');
  text.push(' Requests');
  text.push('----------------------------------------------');
  text.push('');

  this.appendRequestsPrintedAsText_(text);

  // Open a new window to display this text.
  this.setText_(text.join('\n'));
};

DataView.prototype.appendRequestsPrintedAsText_ = function(out) {
  // Concatenate the passively captured events with the actively captured events
  // into a single array.
  var allEvents = g_browser.getAllPassivelyCapturedEvents().concat(
      g_browser.getAllActivelyCapturedEvents());

  // Group the events into buckets by source ID, and buckets by source type.
  var sourceIds = [];
  var sourceIdToEventList = {};
  var sourceTypeToSourceIdList = {}

  for (var i = 0; i < allEvents.length; ++i) {
    var e = allEvents[i];
    var eventList = sourceIdToEventList[e.source.id];
    if (!eventList) {
      eventList = [];
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


  // For each source (ordered by when that source was first started).
  for (var i = 0; i < sourceIds.length; ++i) {
    var sourceId = sourceIds[i];
    var eventList = sourceIdToEventList[sourceId];
    var sourceType = eventList[0].source.type;

    out.push('------------------------------');
    out.push(getKeyWithValue(LogSourceType, sourceType) +
             ' (id=' + sourceId + ')');
    out.push('------------------------------');

    out.push(PrintSourceEntriesAsText(eventList));
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
 * Stringifies |arrayData| to formatted table output, and appends it to the
 * line buffer |out|.
 */
DataView.prototype.appendPrettyPrintedTable_ = function(arrayData, out) {
  for (var i = 0; i < arrayData.length; ++i) {
    var e = arrayData[i];
    var eString = '[' + i + ']: ';
    for (var key in e) {
      eString += key + "=" + e[key] + "; ";
    }
    out.push(eString);
  }
};

