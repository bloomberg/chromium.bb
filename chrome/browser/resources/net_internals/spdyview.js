// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays a summary of the state of each SPDY sessions, and
 * has links to display them in the events tab.
 *
 * @constructor
 */
function SpdyView(mainBoxId, spdySessionNoneSpanId, spdySessionLinkSpanId,
                  spdySessionDivId) {
  DivView.call(this, mainBoxId);
  g_browser.addSpdySessionInfoObserver(this);

  this.spdySessionNoneSpan_ = document.getElementById(spdySessionNoneSpanId);
  this.spdySessionLinkSpan_ = document.getElementById(spdySessionLinkSpanId);
  this.spdySessionDiv_ = document.getElementById(spdySessionDivId);
}

inherits(SpdyView, DivView);

/**
 * If |spdySessionInfo| is not null, displays a single table with information
 * on each SPDY session.  Otherwise, displays "None".
 */
SpdyView.prototype.onSpdySessionInfoChanged = function(spdySessionInfo) {
  this.spdySessionDiv_.innerHTML = '';

  var hasNoSession = (spdySessionInfo == null || spdySessionInfo.length == 0);
  setNodeDisplay(this.spdySessionNoneSpan_, hasNoSession);
  setNodeDisplay(this.spdySessionLinkSpan_, !hasNoSession);

  if (hasNoSession)
    return;

  var tablePrinter = SpdyView.createSessionTablePrinter(spdySessionInfo);
  tablePrinter.toHTML(this.spdySessionDiv_, 'styledTable');
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

