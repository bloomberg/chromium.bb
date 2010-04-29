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
function DataView(mainBoxId, exportJsonButtonId, exportTextButtonId) {
  DivView.call(this, mainBoxId);

  var exportJsonButton = document.getElementById(exportJsonButtonId);
  var exportTextButton = document.getElementById(exportTextButtonId);

  exportJsonButton.onclick = this.onExportToJSON_.bind(this);
  exportTextButton.onclick = this.onExportToText_.bind(this);
}

inherits(DataView, DivView);

/**
 * Very simple output format which directly displays the internally captured
 * data in its javascript format.
 */
DataView.prototype.onExportToJSON_ = function() {
  // Format some text describing the data we have captured.
  var text = [];  // Lines of text.

  text.push('//----------------------------------------------');
  text.push('// Proxy settings');
  text.push('//----------------------------------------------');
  text.push('');

  text.push('proxySettings = ' +
            JSON.stringify(g_browser.proxySettings_.currentData_));

  text.push('');
  text.push('//----------------------------------------------');
  text.push('// Bad proxies');
  text.push('//----------------------------------------------');
  text.push('');

  text.push('badProxies = ' +
            JSON.stringify(g_browser.badProxies_.currentData_));

  text.push('');
  text.push('//----------------------------------------------');
  text.push('// Host resolver cache');
  text.push('//----------------------------------------------');
  text.push('');

  text.push('hostResolverCache = ' +
            JSON.stringify(g_browser.hostResolverCache_.currentData_));

  text.push('');
  text.push('//----------------------------------------------');
  text.push('// Passively captured events');
  text.push('//----------------------------------------------');
  text.push('');

  this.appendPrettyPrintedJsonArray_('passive',
                                     g_browser.getAllPassivelyCapturedEvents(),
                                     text);

  text.push('');
  text.push('//----------------------------------------------');
  text.push('// Actively captured events');
  text.push('//----------------------------------------------');
  text.push('');

  this.appendPrettyPrintedJsonArray_('active',
                                     g_browser.getAllActivelyCapturedEvents(),
                                     text);

  // Open a new window to display this text.
  this.displayPopupWindow_(text.join('\n'));
};

/**
 * Presents the captured data as formatted text.
 */
DataView.prototype.onExportToText_ = function() {
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
  text.push(' URL requests');
  text.push('----------------------------------------------');
  text.push('');

  // TODO(eroman): Output the per-request events.
  text.push('TODO');

  // Open a new window to display this text.
  this.displayPopupWindow_(text.join('\n'));
};

/**
 * Helper function to open a new window and display |text| in it.
 */
DataView.prototype.displayPopupWindow_ = function(text) {
  // Note that we use a data:URL because the chrome:// URL scheme doesn't
  // allow us to mutate any new windows we open.
  dataUrl = 'data:text/plain,' + encodeURIComponent(text);
  window.open(dataUrl, '_blank');
};

/**
 * Stringifies |arrayData| as JSON-like output, and appends it to the
 * line buffer |out|.
 */
DataView.prototype.appendPrettyPrintedJsonArray_ = function(name,
                                                            arrayData,
                                                            out) {
  out.push(name + ' = [];');
  for (var i = 0; i < arrayData.length; ++i) {
    out.push(name + '[' + i + '] = ' + JSON.stringify(arrayData[i]) + ';');
  }
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

