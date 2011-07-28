// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays a summary of the state of each SPDY sessions, and
 * has links to display them in the events tab.
 *
 * @constructor
 */
function SpdyView() {
  const mainBoxId = 'spdyTabContent';
  const spdyEnabledSpanId = 'spdyEnabledSpan';
  const spdyUseAlternateProtocolSpanId = 'spdyUseAlternateProtocolSpan';
  const spdyForceAlwaysSpanId = 'spdyForceAlwaysSpan';
  const spdyForceOverSslSpanId = 'spdyForceOverSslSpan';
  const spdyNextProtocolsSpanId = 'spdyNextProtocolsSpan';
  const spdyAlternateProtocolMappingsDivId = 'spdyAlternateProtocolMappingsDiv';
  const spdySessionNoneSpanId = 'spdySessionNoneSpan';
  const spdySessionLinkSpanId = 'spdySessionLinkSpan';
  const spdySessionDivId = 'spdySessionDiv';

  DivView.call(this, mainBoxId);
  g_browser.addSpdySessionInfoObserver(this);
  g_browser.addSpdyStatusObserver(this);
  g_browser.addSpdyAlternateProtocolMappingsObserver(this);

  this.spdyEnabledSpan_ = $(spdyEnabledSpanId);
  this.spdyUseAlternateProtocolSpan_ = $(spdyUseAlternateProtocolSpanId);
  this.spdyForceAlwaysSpan_ = $(spdyForceAlwaysSpanId);
  this.spdyForceOverSslSpan_ = $(spdyForceOverSslSpanId);
  this.spdyNextProtocolsSpan_ = $(spdyNextProtocolsSpanId);

  this.spdyAlternateProtocolMappingsDiv_ =
      $(spdyAlternateProtocolMappingsDivId);
  this.spdySessionNoneSpan_ = $(spdySessionNoneSpanId);
  this.spdySessionLinkSpan_ = $(spdySessionLinkSpanId);
  this.spdySessionDiv_ = $(spdySessionDivId);
}

inherits(SpdyView, DivView);

SpdyView.prototype.onLoadLogFinish = function(data) {
  return this.onSpdySessionInfoChanged(data.spdySessionInfo) &&
         this.onSpdyStatusChanged(data.spdyStatus) &&
         this.onSpdyAlternateProtocolMappingsChanged(
             data.spdyAlternateProtocolMappings);
};

/**
 * If |spdySessionInfo| there are any sessions, display a single table with
 * information on each SPDY session.  Otherwise, displays "None".
 */
SpdyView.prototype.onSpdySessionInfoChanged = function(spdySessionInfo) {
  this.spdySessionDiv_.innerHTML = '';

  var hasNoSession = (spdySessionInfo == null || spdySessionInfo.length == 0);
  setNodeDisplay(this.spdySessionNoneSpan_, hasNoSession);
  setNodeDisplay(this.spdySessionLinkSpan_, !hasNoSession);

  // Only want to be hide the tab if there's no data.  In the case of having
  // data but no sessions, still show the tab.
  if (!spdySessionInfo)
    return false;

  if (!hasNoSession) {
    var tablePrinter = SpdyView.createSessionTablePrinter(spdySessionInfo);
    tablePrinter.toHTML(this.spdySessionDiv_, 'styledTable');
  }

  return true;
};

/**
 * Displays information on the global SPDY status.
 */
SpdyView.prototype.onSpdyStatusChanged = function(spdyStatus) {
  this.spdyEnabledSpan_.textContent = spdyStatus.spdy_enabled;
  this.spdyUseAlternateProtocolSpan_.textContent =
      spdyStatus.use_alternate_protocols;
  this.spdyForceAlwaysSpan_.textContent = spdyStatus.force_spdy_always;
  this.spdyForceOverSslSpan_.textContent = spdyStatus.force_spdy_over_ssl;
  this.spdyNextProtocolsSpan_.textContent = spdyStatus.next_protos;

  return true;
};

/**
 * If |spdyAlternateProtocolMappings| is not empty, displays a single table
 * with information on each alternate protocol enabled server.  Otherwise,
 * displays "None".
 */
SpdyView.prototype.onSpdyAlternateProtocolMappingsChanged =
    function(spdyAlternateProtocolMappings) {

  this.spdyAlternateProtocolMappingsDiv_.innerHTML = '';

  if (spdyAlternateProtocolMappings != null &&
      spdyAlternateProtocolMappings.length > 0) {
    var tabPrinter = SpdyView.createAlternateProtocolMappingsTablePrinter(
            spdyAlternateProtocolMappings);
    tabPrinter.toHTML(this.spdyAlternateProtocolMappingsDiv_, 'styledTable');
  } else {
    this.spdyAlternateProtocolMappingsDiv_.innerHTML = 'None';
  }
  return true;
};

/**
 * Creates a table printer to print out the state of list of SPDY sessions.
 */
SpdyView.createSessionTablePrinter = function(spdySessions) {
  var tablePrinter = new TablePrinter();
  tablePrinter.addHeaderCell('Host');
  tablePrinter.addHeaderCell('Proxy');
  tablePrinter.addHeaderCell('ID');
  tablePrinter.addHeaderCell('Active streams');
  tablePrinter.addHeaderCell('Unclaimed pushed');
  tablePrinter.addHeaderCell('Max');
  tablePrinter.addHeaderCell('Initiated');
  tablePrinter.addHeaderCell('Pushed');
  tablePrinter.addHeaderCell('Pushed and claimed');
  tablePrinter.addHeaderCell('Abandoned');
  tablePrinter.addHeaderCell('Received frames');
  tablePrinter.addHeaderCell('Secure');
  tablePrinter.addHeaderCell('Sent settings');
  tablePrinter.addHeaderCell('Received settings');
  tablePrinter.addHeaderCell('Error');

  for (var i = 0; i < spdySessions.length; i++) {
    var session = spdySessions[i];
    tablePrinter.addRow();

    tablePrinter.addCell(session.host_port_pair);
    tablePrinter.addCell(session.proxy);

    var idCell = tablePrinter.addCell(session.source_id);
    idCell.link = '#events&q=id:' + session.source_id;

    tablePrinter.addCell(session.active_streams);
    tablePrinter.addCell(session.unclaimed_pushed_streams);
    tablePrinter.addCell(session.max_concurrent_streams);
    tablePrinter.addCell(session.streams_initiated_count);
    tablePrinter.addCell(session.streams_pushed_count);
    tablePrinter.addCell(session.streams_pushed_and_claimed_count);
    tablePrinter.addCell(session.streams_abandoned_count);
    tablePrinter.addCell(session.frames_received);
    tablePrinter.addCell(session.is_secure);
    tablePrinter.addCell(session.sent_settings);
    tablePrinter.addCell(session.received_settings);
    tablePrinter.addCell(session.error);
  }
  return tablePrinter;
};


/**
 * Creates a table printer to print out the list of alternate protocol
 * mappings.
 */
SpdyView.createAlternateProtocolMappingsTablePrinter =
    function(spdyAlternateProtocolMappings) {
  var tablePrinter = new TablePrinter();
  tablePrinter.addHeaderCell('Host');
  tablePrinter.addHeaderCell('Alternate Protocol');

  for (var i = 0; i < spdyAlternateProtocolMappings.length; i++) {
    var entry = spdyAlternateProtocolMappings[i];
    tablePrinter.addRow();

    tablePrinter.addCell(entry.host_port_pair);
    tablePrinter.addCell(entry.alternate_protocol);
  }
  return tablePrinter;
};

