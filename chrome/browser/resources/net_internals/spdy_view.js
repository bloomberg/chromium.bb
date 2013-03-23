// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays a summary of the state of each SPDY sessions, and
 * has links to display them in the events tab.
 */
var SpdyView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function SpdyView() {
    assertFirstConstructorCall(SpdyView);

    // Call superclass's constructor.
    superClass.call(this, SpdyView.MAIN_BOX_ID);

    g_browser.addSpdySessionInfoObserver(this, true);
    g_browser.addSpdyStatusObserver(this, true);
    g_browser.addSpdyAlternateProtocolMappingsObserver(this, true);

    this.spdyEnabledSpan_ = $(SpdyView.ENABLED_SPAN_ID);
    this.spdyUseAlternateProtocolSpan_ =
        $(SpdyView.USE_ALTERNATE_PROTOCOL_SPAN_ID);
    this.spdyForceAlwaysSpan_ = $(SpdyView.FORCE_ALWAYS_SPAN_ID);
    this.spdyForceOverSslSpan_ = $(SpdyView.FORCE_OVER_SSL_SPAN_ID);
    this.spdyNextProtocolsSpan_ = $(SpdyView.NEXT_PROTOCOLS_SPAN_ID);

    this.spdyAlternateProtocolMappingsDiv_ =
        $(SpdyView.ALTERNATE_PROTOCOL_MAPPINGS_DIV_ID);
    this.spdySessionNoneSpan_ = $(SpdyView.SESSION_NONE_SPAN_ID);
    this.spdySessionLinkSpan_ = $(SpdyView.SESSION_LINK_SPAN_ID);
    this.spdySessionDiv_ = $(SpdyView.SESSION_DIV_ID);
  }

  // ID for special HTML element in category_tabs.html
  SpdyView.TAB_HANDLE_ID = 'tab-handle-spdy';

  // IDs for special HTML elements in spdy_view.html
  SpdyView.MAIN_BOX_ID = 'spdy-view-tab-content';
  SpdyView.ENABLED_SPAN_ID = 'spdy-view-enabled-span';
  SpdyView.USE_ALTERNATE_PROTOCOL_SPAN_ID =
      'spdy-view-alternate-protocol-span';
  SpdyView.FORCE_ALWAYS_SPAN_ID = 'spdy-view-force-always-span';
  SpdyView.FORCE_OVER_SSL_SPAN_ID = 'spdy-view-force-over-ssl-span';
  SpdyView.NEXT_PROTOCOLS_SPAN_ID = 'spdy-view-next-protocols-span';
  SpdyView.ALTERNATE_PROTOCOL_MAPPINGS_DIV_ID =
      'spdy-view-alternate-protocol-mappings-div';
  SpdyView.SESSION_NONE_SPAN_ID = 'spdy-view-session-none-span';
  SpdyView.SESSION_LINK_SPAN_ID = 'spdy-view-session-link-span';
  SpdyView.SESSION_DIV_ID = 'spdy-view-session-div';

  cr.addSingletonGetter(SpdyView);

  SpdyView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onSpdySessionInfoChanged(data.spdySessionInfo) &&
             this.onSpdyStatusChanged(data.spdyStatus) &&
             this.onSpdyAlternateProtocolMappingsChanged(
                 data.spdyAlternateProtocolMappings);
    },

    /**
     * If |spdySessionInfo| there are any sessions, display a single table with
     * information on each SPDY session.  Otherwise, displays "None".
     */
    onSpdySessionInfoChanged: function(spdySessionInfo) {
      this.spdySessionDiv_.innerHTML = '';

      var hasNoSession =
          (spdySessionInfo == null || spdySessionInfo.length == 0);
      setNodeDisplay(this.spdySessionNoneSpan_, hasNoSession);
      setNodeDisplay(this.spdySessionLinkSpan_, !hasNoSession);

      // Only want to be hide the tab if there's no data.  In the case of having
      // data but no sessions, still show the tab.
      if (!spdySessionInfo)
        return false;

      if (!hasNoSession) {
        var tablePrinter = createSessionTablePrinter(spdySessionInfo);
        tablePrinter.toHTML(this.spdySessionDiv_, 'styled-table');
      }

      return true;
    },

    /**
     * Displays information on the global SPDY status.
     */
    onSpdyStatusChanged: function(spdyStatus) {
      this.spdyEnabledSpan_.textContent = spdyStatus.spdy_enabled;
      this.spdyUseAlternateProtocolSpan_.textContent =
          spdyStatus.use_alternate_protocols;
      this.spdyForceAlwaysSpan_.textContent = spdyStatus.force_spdy_always;
      this.spdyForceOverSslSpan_.textContent = spdyStatus.force_spdy_over_ssl;
      this.spdyNextProtocolsSpan_.textContent = spdyStatus.next_protos;

      return true;
    },

    /**
     * If |spdyAlternateProtocolMappings| is not empty, displays a single table
     * with information on each alternate protocol enabled server.  Otherwise,
     * displays "None".
     */
    onSpdyAlternateProtocolMappingsChanged:
        function(spdyAlternateProtocolMappings) {

      this.spdyAlternateProtocolMappingsDiv_.innerHTML = '';

      if (spdyAlternateProtocolMappings != null &&
          spdyAlternateProtocolMappings.length > 0) {
        var tabPrinter = createAlternateProtocolMappingsTablePrinter(
                spdyAlternateProtocolMappings);
        tabPrinter.toHTML(
            this.spdyAlternateProtocolMappingsDiv_, 'styled-table');
      } else {
        this.spdyAlternateProtocolMappingsDiv_.innerHTML = 'None';
      }
      return true;
    }
  };

  /**
   * Creates a table printer to print out the state of list of SPDY sessions.
   */
  function createSessionTablePrinter(spdySessions) {
    var tablePrinter = new TablePrinter();
    tablePrinter.addHeaderCell('Host');
    tablePrinter.addHeaderCell('Proxy');
    tablePrinter.addHeaderCell('ID');
    tablePrinter.addHeaderCell('Protocol Negotiated');
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
    tablePrinter.addHeaderCell('Send window');
    tablePrinter.addHeaderCell('Receive window');
    tablePrinter.addHeaderCell('Unacked received data');
    tablePrinter.addHeaderCell('Error');

    for (var i = 0; i < spdySessions.length; i++) {
      var session = spdySessions[i];
      tablePrinter.addRow();

      var host = session.host_port_pair;
      if (session.aliases)
        host += ' ' + session.aliases.join(' ');
      tablePrinter.addCell(host);
      tablePrinter.addCell(session.proxy);

      var idCell = tablePrinter.addCell(session.source_id);
      idCell.link = '#events&q=id:' + session.source_id;

      tablePrinter.addCell(session.protocol_negotiated);
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
      tablePrinter.addCell(session.send_window_size);
      tablePrinter.addCell(session.recv_window_size);
      tablePrinter.addCell(session.unacked_recv_window_bytes);
      tablePrinter.addCell(session.error);
    }
    return tablePrinter;
  }

  /**
   * Creates a table printer to print out the list of alternate protocol
   * mappings.
   */
  function createAlternateProtocolMappingsTablePrinter(
      spdyAlternateProtocolMappings) {
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
  }

  return SpdyView;
})();
