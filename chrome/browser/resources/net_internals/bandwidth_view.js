// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** This view displays summary statistics on bandwidth usage. */
var BandwidthView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function BandwidthView() {
    assertFirstConstructorCall(BandwidthView);

    // Call superclass's constructor.
    superClass.call(this, BandwidthView.MAIN_BOX_ID);

    g_browser.addSessionNetworkStatsObserver(this, true);
    g_browser.addHistoricNetworkStatsObserver(this, true);

    this.bandwidthUsageTable_ = $(BandwidthView.BANDWIDTH_USAGE_TABLE);
    this.sessionNetworkStats_ = null;
    this.historicNetworkStats_ = null;
  }

  // ID for special HTML element in category_tabs.html
  BandwidthView.TAB_HANDLE_ID = 'tab-handle-bandwidth';

  // IDs for special HTML elements in bandwidth_view.html
  BandwidthView.MAIN_BOX_ID = 'bandwidth-view-tab-content';
  BandwidthView.BANDWIDTH_USAGE_TABLE = 'bandwidth-usage-table';

  cr.addSingletonGetter(BandwidthView);

  BandwidthView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onSessionNetworkStatsChanged(data.sessionNetworkStats) &&
          this.onHistoricNetworkStatsChanged(data.historicNetworkStats);
    },

    /**
     * Retains information on bandwidth usage this session.
     */
    onSessionNetworkStatsChanged: function(sessionNetworkStats) {
      this.sessionNetworkStats_ = sessionNetworkStats;
      this.updateBandwidthUsageTable();
      return true;
    },

    /**
     * Displays information on bandwidth usage this session and over the
     * browser's lifetime.
     */
    onHistoricNetworkStatsChanged: function(historicNetworkStats) {
      this.historicNetworkStats_ = historicNetworkStats;
      this.updateBandwidthUsageTable();
      return true;
    },

    /**
     * Update the bandwidth usage table.
     */
    updateBandwidthUsageTable: function() {
      this.bandwidthUsageTable_.innerHTML = '';
      var tabPrinter = createBandwidthUsageTablePrinter(
          this.sessionNetworkStats_, this.historicNetworkStats_);
      tabPrinter.toHTML(this.bandwidthUsageTable_, 'styled-table');
      return true;
    }
  };

  /**
   * Adds a row of bandwidth usage statistics to the bandwidth usage table.
   */
  function addStatsRow(tablePrinter, name, receivedLength, originalLength) {
    var difference = originalLength - receivedLength;
    var percent = 0;
    if (originalLength > 0) {
      percent = Math.floor(difference * 100 / originalLength);
    }
    tablePrinter.addRow();
    tablePrinter.addCell(name);
    tablePrinter.addCell(receivedLength);
    tablePrinter.addCell(originalLength);
    tablePrinter.addCell(difference);
    tablePrinter.addCell(percent);
  }

  /**
   * Creates a table printer to print out the bandwidth usage statistics.
   */
  function createBandwidthUsageTablePrinter(sessionNetworkStats,
                                            historicNetworkStats) {
    var tablePrinter = new TablePrinter();
    tablePrinter.addHeaderCell('');
    tablePrinter.addHeaderCell('Received Content Length (Bytes)');
    tablePrinter.addHeaderCell('Original Content Length (Bytes)');
    tablePrinter.addHeaderCell('Savings (Bytes)');
    tablePrinter.addHeaderCell('Savings (%)');
    if (sessionNetworkStats != null) {
      addStatsRow(tablePrinter, 'Session',
          sessionNetworkStats.session_received_content_length,
          sessionNetworkStats.session_original_content_length);
    }
    if (historicNetworkStats != null) {
      addStatsRow(tablePrinter, 'Total',
          historicNetworkStats.historic_received_content_length,
          historicNetworkStats.historic_original_content_length);
    }
    return tablePrinter;
  }

  return BandwidthView;
})();
