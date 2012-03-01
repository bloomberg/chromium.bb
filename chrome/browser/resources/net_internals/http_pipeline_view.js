// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays a summary of the state of each HTTP pipelined connection,
 * and has links to display them in the events tab.
 */
var HttpPipelineView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function HttpPipelineView() {
    assertFirstConstructorCall(HttpPipelineView);

    // Call superclass's constructor.
    superClass.call(this, HttpPipelineView.MAIN_BOX_ID);

    g_browser.addHttpPipeliningStatusObserver(this, true);

    this.httpPipeliningEnabledSpan_ = $(HttpPipelineView.ENABLED_SPAN_ID);
    this.httpPipelineConnectionsNoneSpan_ =
        $(HttpPipelineView.CONNECTIONS_NONE_SPAN_ID);
    this.httpPipelineConnectionsLinkSpan_ =
        $(HttpPipelineView.CONNECTIONS_LINK_SPAN_ID);
    this.httpPipelineConnectionsDiv_ = $(HttpPipelineView.CONNECTIONS_DIV_ID);
    this.httpPipelineKnownHostsDiv_ = $(HttpPipelineView.KNOWN_HOSTS_DIV_ID);
  }

  // ID for special HTML element in category_tabs.html
  HttpPipelineView.TAB_HANDLE_ID = 'tab-handle-http-pipeline';

  // IDs for special HTML elements in http_pipeline_view.html
  HttpPipelineView.MAIN_BOX_ID = 'http-pipeline-view-tab-content';
  HttpPipelineView.ENABLED_SPAN_ID = 'http-pipeline-view-enabled-span';
  HttpPipelineView.CONNECTIONS_NONE_SPAN_ID =
      'http-pipeline-view-connections-none-span';
  HttpPipelineView.CONNECTIONS_LINK_SPAN_ID =
      'http-pipeline-view-connections-link-span';
  HttpPipelineView.CONNECTIONS_DIV_ID = 'http-pipeline-view-connections-div';
  HttpPipelineView.KNOWN_HOSTS_DIV_ID = 'http-pipeline-view-known-hosts-div';

  cr.addSingletonGetter(HttpPipelineView);

  HttpPipelineView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onHttpPipeliningStatusChanged(data.httpPipeliningStatus);
    },

    /**
     * Displays information on the global HTTP pipelining status.
     */
    onHttpPipeliningStatusChanged: function(httpPipelineStatus) {
      return this.displayHttpPipeliningEnabled(httpPipelineStatus) &&
             this.displayHttpPipelinedConnectionInfo(
                 httpPipelineStatus.pipelined_connection_info) &&
             this.displayHttpPipeliningKnownHosts(
                 httpPipelineStatus.pipelined_host_info);
    },

    displayHttpPipeliningEnabled: function(httpPipelineStatus) {
      this.httpPipeliningEnabledSpan_.textContent =
          httpPipelineStatus.pipelining_enabled;

      return httpPipelineStatus.pipelining_enabled;
    },

    /**
     * If |httpPipelinedConnectionInfo| is not empty, then display information
     * on each HTTP pipelined connection.  Otherwise, displays "None".
     */
    displayHttpPipelinedConnectionInfo:
        function(httpPipelinedConnectionInfo) {
      this.httpPipelineConnectionsDiv_.innerHTML = '';

      var hasInfo = (httpPipelinedConnectionInfo != null &&
                     httpPipelinedConnectionInfo.length > 0);
      setNodeDisplay(this.httpPipelineConnectionsNoneSpan_, !hasInfo);
      setNodeDisplay(this.httpPipelineConnectionsLinkSpan_, hasInfo);

      if (hasInfo) {
        var tablePrinter = createConnectionTablePrinter(
            httpPipelinedConnectionInfo);
        tablePrinter.toHTML(this.httpPipelineConnectionsDiv_, 'styled-table');
      }

      return true;
    },

    /**
     * If |httpPipeliningKnownHosts| is not empty, displays a single table
     * with information on known pipelining hosts.  Otherwise, displays "None".
     */
    displayHttpPipeliningKnownHosts: function(httpPipeliningKnownHosts) {
      this.httpPipelineKnownHostsDiv_.innerHTML = '';

      if (httpPipeliningKnownHosts != null &&
          httpPipeliningKnownHosts.length > 0) {
        var tabPrinter = createKnownHostsTablePrinter(httpPipeliningKnownHosts);
        tabPrinter.toHTML(
            this.httpPipelineKnownHostsDiv_, 'styled-table');
      } else {
        this.httpPipelineKnownHostsDiv_.innerHTML = 'None';
      }
      return true;
    }
  };

  /**
   * Creates a table printer to print out the state of a list of HTTP pipelined
   * connections.
   */
  function createConnectionTablePrinter(httpPipelinedConnectionInfo) {
    var tablePrinter = new TablePrinter();
    tablePrinter.addHeaderCell('Host');
    tablePrinter.addHeaderCell('Forced');
    tablePrinter.addHeaderCell('Depth');
    tablePrinter.addHeaderCell('Capacity');
    tablePrinter.addHeaderCell('Usable');
    tablePrinter.addHeaderCell('Active');
    tablePrinter.addHeaderCell('ID');

    for (var i = 0; i < httpPipelinedConnectionInfo.length; i++) {
      var host = httpPipelinedConnectionInfo[i];
      for (var j = 0; j < host.length; j++) {
        var connection = host[j];
        tablePrinter.addRow();

        tablePrinter.addCell(connection.host);
        tablePrinter.addCell(
            connection.forced === undefined ? false : connection.forced);
        tablePrinter.addCell(connection.depth);
        tablePrinter.addCell(connection.capacity);
        tablePrinter.addCell(connection.usable);
        tablePrinter.addCell(connection.active);

        var idCell = tablePrinter.addCell(connection.source_id);
        idCell.link = '#events&q=id:' + connection.source_id;
      }
    }
    return tablePrinter;
  }

  /**
   * Creates a table printer to print out the list of known hosts and whether or
   * not they support pipelining.
   */
  function createKnownHostsTablePrinter(httpPipeliningKnownHosts) {
    var tablePrinter = new TablePrinter();
    tablePrinter.addHeaderCell('Host');
    tablePrinter.addHeaderCell('Pipelining Capalibility');

    for (var i = 0; i < httpPipeliningKnownHosts.length; i++) {
      var entry = httpPipeliningKnownHosts[i];
      tablePrinter.addRow();

      tablePrinter.addCell(entry.host);
      tablePrinter.addCell(entry.capability);
    }
    return tablePrinter;
  }

  return HttpPipelineView;
})();
