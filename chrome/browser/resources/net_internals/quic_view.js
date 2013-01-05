// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays a summary of the state of each QUIC session, and
 * has links to display them in the events tab.
 */
var QuicView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function QuicView() {
    assertFirstConstructorCall(QuicView);

    // Call superclass's constructor.
    superClass.call(this, QuicView.MAIN_BOX_ID);

    g_browser.addQuicInfoObserver(this, true);

    this.quicEnabledSpan_ = $(QuicView.ENABLED_SPAN_ID);
    this.quicForcePortSpan_ = $(QuicView.FORCE_PORT_SPAN_ID);

    this.quicSessionNoneSpan_ = $(QuicView.SESSION_NONE_SPAN_ID);
    this.quicSessionLinkSpan_ = $(QuicView.SESSION_LINK_SPAN_ID);
    this.quicSessionDiv_ = $(QuicView.SESSION_DIV_ID);
  }

  // ID for special HTML element in category_tabs.html
  QuicView.TAB_HANDLE_ID = 'tab-handle-quic';

  // IDs for special HTML elements in quic_view.html
  QuicView.MAIN_BOX_ID = 'quic-view-tab-content';
  QuicView.ENABLED_SPAN_ID = 'quic-view-enabled-span';
  QuicView.FORCE_PORT_SPAN_ID = 'quic-view-force-port-span';
  QuicView.SESSION_NONE_SPAN_ID = 'quic-view-session-none-span';
  QuicView.SESSION_LINK_SPAN_ID = 'quic-view-session-link-span';
  QuicView.SESSION_DIV_ID = 'quic-view-session-div';

  cr.addSingletonGetter(QuicView);

  QuicView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onQuicInfoChanged(data.quicInfo);
    },

    /**
     * If there are any sessions, display a single table with
     * information on each QUIC session.  Otherwise, displays "None".
     */
    onQuicInfoChanged: function(quicInfo) {
      this.quicSessionDiv_.innerHTML = '';

      var hasNoSession =
          (!quicInfo || !quicInfo.sessions || quicInfo.sessions.length == 0);
      setNodeDisplay(this.quicSessionNoneSpan_, hasNoSession);
      setNodeDisplay(this.quicSessionLinkSpan_, !hasNoSession);

      // Only want to be hide the tab if there's no data.  In the case of having
      // data but no sessions, still show the tab.
      if (!quicInfo)
        return false;

      this.quicEnabledSpan_.textContent = quicInfo.quic_enabled;
      this.quicForcePortSpan_.textContent =
          quicInfo.origin_port_to_force_quic_on;

      if (!hasNoSession) {
        var tablePrinter = createSessionTablePrinter(quicInfo.sessions);
        tablePrinter.toHTML(this.quicSessionDiv_, 'styled-table');
      }

      return true;
    },
  };

  /**
   * Creates a table printer to print out the state of list of QUIC sessions.
   */
  function createSessionTablePrinter(quicSessions) {
    var tablePrinter = new TablePrinter();

    tablePrinter.addHeaderCell('Host');
    tablePrinter.addHeaderCell('Peer address');
    tablePrinter.addHeaderCell('GUID');
    tablePrinter.addHeaderCell('Active streams');

    for (var i = 0; i < quicSessions.length; i++) {
      var session = quicSessions[i];
      tablePrinter.addRow();

      var host = session.host_port_pair;
      if (session.aliases)
        host += ' ' + session.aliases.join(' ');
      tablePrinter.addCell(host);

      tablePrinter.addCell(session.peer_address);
      tablePrinter.addCell(session.guid);
      tablePrinter.addCell(session.open_streams);
    }
    return tablePrinter;
  }

  return QuicView;
})();
