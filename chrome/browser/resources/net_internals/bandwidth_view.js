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

    // Register to receive data reduction proxy info.
    g_browser.addDataReductionProxyInfoObserver(this, true);

    // Register to receive bad proxy info.
    g_browser.addBadProxiesObserver(this, true);

    this.sessionNetworkStats_ = null;
    this.historicNetworkStats_ = null;
  }

  BandwidthView.TAB_ID = 'tab-handle-bandwidth';
  BandwidthView.TAB_NAME = 'Bandwidth';
  BandwidthView.TAB_HASH = '#bandwidth';

  // IDs for special HTML elements in bandwidth_view.html
  BandwidthView.MAIN_BOX_ID = 'bandwidth-view-tab-content';
  BandwidthView.ENABLED_ID = 'data-reduction-proxy-enabled';
  BandwidthView.PRIMARY_PROXY_ID = 'data-reduction-proxy-primary';
  BandwidthView.SECONDARY_PROXY_ID = 'data-reduction-proxy-secondary';
  BandwidthView.PROBE_STATUS_ID = 'data-reduction-proxy-probe-status';
  BandwidthView.BYPASS_STATE_ID = 'data-reduction-proxy-bypass-state';
  BandwidthView.BYPASS_STATE_CONTAINER_ID =
      'data-reduction-proxy-bypass-state-container';
  BandwidthView.EVENTS_TBODY_ID = 'data-reduction-proxy-view-events-tbody';
  BandwidthView.EVENTS_UL = 'data-reduction-proxy-view-events-list';
  BandwidthView.STATS_BOX_ID = 'bandwidth-stats-table';

  cr.addSingletonGetter(BandwidthView);

  BandwidthView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    data_reduction_proxy_config_: null,
    last_bypass_: null,
    bad_proxy_config_: null,

    onLoadLogFinish: function(data) {
      return this.onBadProxiesChanged(data.badProxies) &&
          this.onDataReductionProxyInfoChanged(data.dataReductionProxyInfo) &&
          (this.onSessionNetworkStatsChanged(data.sessionNetworkStats) ||
              this.onHistoricNetworkStatsChanged(data.historicNetworkStats));
    },

    /**
     * Retains information on bandwidth usage this session.
     */
    onSessionNetworkStatsChanged: function(sessionNetworkStats) {
      this.sessionNetworkStats_ = sessionNetworkStats;
      return this.updateBandwidthUsageTable_();
    },

    /**
     * Displays information on bandwidth usage this session and over the
     * browser's lifetime.
     */
    onHistoricNetworkStatsChanged: function(historicNetworkStats) {
      this.historicNetworkStats_ = historicNetworkStats;
      return this.updateBandwidthUsageTable_();
    },

    /**
     * Updates the UI based on receiving changes in information about the
     * data reduction proxy summary.
     */
    onDataReductionProxyInfoChanged: function(info) {
      $(BandwidthView.EVENTS_TBODY_ID).innerHTML = '';

      if (!info)
        return false;

      if (info.enabled) {
        $(BandwidthView.ENABLED_ID).innerText = 'Enabled';
        $(BandwidthView.PROBE_STATUS_ID).innerText =
            info.probe != null ? info.probe : 'N/A';
        this.last_bypass_ = info.last_bypass != null ?
            this.parseBypassEvent_(info.last_bypass) : null;
        this.data_reduction_proxy_config_ = info.proxy_config.params;
      } else {
        $(BandwidthView.ENABLED_ID).innerText = 'Disabled';
        $(BandwidthView.PROBE_STATUS_ID).innerText = 'N/A';
        this.data_reduction_proxy_config_ = null;
      }

      this.updateDataReductionProxyConfig_();

      for (var eventIndex = info.events.length - 1; eventIndex >= 0;
          --eventIndex) {
        var event = info.events[eventIndex];
        var headerRow = addNode($(BandwidthView.EVENTS_TBODY_ID), 'tr');
        var detailsRow = addNode($(BandwidthView.EVENTS_TBODY_ID), 'tr');

        var timeCell = addNode(headerRow, 'td');
        var actionCell = addNode(headerRow, 'td');
        var detailsCell = addNode(detailsRow, 'td');
        detailsCell.colSpan = 2;
        detailsCell.className = 'data-reduction-proxy-view-events-details';
        var eventTime = timeutil.convertTimeTicksToDate(event.time);
        timeutil.addNodeWithDate(timeCell, eventTime);

        switch (event.type) {
          case EventType.DATA_REDUCTION_PROXY_ENABLED:
            this.buildEnabledRow_(event, actionCell, detailsCell);
            break;
          case EventType.DATA_REDUCTION_PROXY_CANARY_REQUEST:
            this.buildProbeRow_(event, actionCell, detailsCell);
            break;
          case EventType.DATA_REDUCTION_PROXY_CANARY_RESPONSE_RECEIVED:
            this.buildProbeResponseRow_(event, actionCell, detailsCell);
            break;
          case EventType.DATA_REDUCTION_PROXY_BYPASS_REQUESTED:
            this.buildBypassRow_(event, actionCell, detailsCell);
            break;
          case EventType.DATA_REDUCTION_PROXY_FALLBACK:
            this.buildFallbackRow_(event, actionCell, detailsCell);
            break;
        }
      }

      return true;
    },

    /**
     * Updates the UI based on receiving changes in information about bad
     * proxy servers.
     */
    onBadProxiesChanged: function(badProxies) {
      if (!badProxies)
        return false;

      var newBadProxies = [];
      if (badProxies.length == 0) {
        this.last_bypass_ = null;
      } else {
        for (var i = 0; i < badProxies.length; ++i) {
          var entry = badProxies[i];
          newBadProxies[entry.proxy_uri] = entry.bad_until;
        }
      }
      this.bad_proxy_config_ = newBadProxies;
      this.updateDataReductionProxyConfig_();

      return true;
    },

    /**
     * Update the bandwidth usage table.  Returns false on failure.
     */
    updateBandwidthUsageTable_: function() {
      var sessionNetworkStats = this.sessionNetworkStats_;
      var historicNetworkStats = this.historicNetworkStats_;
      if (!sessionNetworkStats || !historicNetworkStats)
        return false;

      var sessionOriginal = sessionNetworkStats.session_original_content_length;
      var sessionReceived = sessionNetworkStats.session_received_content_length;
      var historicOriginal =
          historicNetworkStats.historic_original_content_length;
      var historicReceived =
          historicNetworkStats.historic_received_content_length;

      var rows = [];
      rows.push({
          title: 'Original (KB)',
          sessionValue: bytesToRoundedKilobytes_(sessionOriginal),
          historicValue: bytesToRoundedKilobytes_(historicOriginal)
      });
      rows.push({
          title: 'Received (KB)',
          sessionValue: bytesToRoundedKilobytes_(sessionReceived),
          historicValue: bytesToRoundedKilobytes_(historicReceived)
      });
      rows.push({
          title: 'Savings (KB)',
          sessionValue:
              bytesToRoundedKilobytes_(sessionOriginal - sessionReceived),
          historicValue:
              bytesToRoundedKilobytes_(historicOriginal - historicReceived)
      });
      rows.push({
          title: 'Savings (%)',
          sessionValue: getPercentSavings_(sessionOriginal, sessionReceived),
          historicValue: getPercentSavings_(historicOriginal,
                                            historicReceived)
      });

      var input = new JsEvalContext({rows: rows});
      jstProcess(input, $(BandwidthView.STATS_BOX_ID));
      return true;
    },

    /**
     * Renders a data reduction proxy enabled/disabled event into the event
     * tbody.
     */
    buildEnabledRow_: function(event, actionCell, detailsCell) {
      if (event.params.enabled == 1) {
        addTextNode(actionCell, 'Proxy: Enabled');
        var proxyWrapper = addNode(detailsCell, 'div');
        addNodeWithText(proxyWrapper, 'div', 'Proxy configuration:');

        if (event.params.primary_origin != null &&
            event.params.primary_origin.trim() != '') {
          var proxyText = 'Primary: ' + event.params.primary_origin;
          if (event.params.primary_restricted != null &&
              event.params.primary_restricted) {
            proxyText += ' (restricted)';
          }
          addNodeWithText(proxyWrapper, 'div', proxyText);
        }

        if (event.params.fallback_origin != null &&
            event.params.fallback_origin.trim() != '') {
          var proxyText = 'Fallback: ' + event.params.fallback_origin;
          if (event.params.fallback_restricted != null &&
              event.params.fallback_restricted) {
            proxyText += ' (restricted)';
          }
          addNodeWithText(proxyWrapper, 'div', proxyText);
        }

        if (event.params.ssl_origin != null &&
            event.params.ssl_origin.trim() != '') {
          addNodeWithText(proxyWrapper, 'div',
              'SSL: ' + event.params.ssl_origin);
        }
      } else {
        addTextNode(actionCell, 'Proxy: Disabled');
      }
    },

    /**
     * Renders a Data Reduction Proxy probe request event into the event
     * tbody.
     */
    buildProbeRow_: function(event, actionCell, detailsCell) {
      if (event.phase == EventPhase.PHASE_BEGIN) {
        addTextNode(actionCell, 'Probe request sent');
        addTextNode(detailsCell, 'URL: ' + event.params.url);
      } else if (event.phase == EventPhase.PHASE_END) {
        addTextNode(actionCell, 'Probe request completed');
        if (event.params.net_error == 0) {
          addTextNode(detailsCell, 'Result: OK');
        } else {
          addTextNode(detailsCell,
                      'Result: ' + netErrorToString(event.params.net_error));
        }
      }
    },

    /**
     * Renders a Data Reduction Proxy probe response event into the event
     * tbody.
     */
    buildProbeResponseRow_: function(event, actionCell, detailsCell) {
      addTextNode(actionCell, 'Probe response received');
    },

    /**
     * Renders a data reduction proxy bypass event into the event tbody.
     */
    buildBypassRow_: function(event, actionCell, detailsCell) {
      var parsedBypass = this.parseBypassEvent_(event);

      addTextNode(actionCell,
                  'Bypass received (' + parsedBypass.bypass_reason + ')');
      var bypassWrapper = addNode(detailsCell, 'div');
      addNodeWithText(bypassWrapper, 'div', 'URL: ' + parsedBypass.origin_url);
      addNodeWithText(
          bypassWrapper, 'div',
          'Bypassed for ' + parsedBypass.bypass_duration_seconds + ' seconds.');
    },

    /**
     * Renders a data reduction proxy fallback event into the event tbody.
     */
    buildFallbackRow_: function(event, actionCell, detailsCell) {
      addTextNode(actionCell, 'Proxy fallback');
    },

    /**
     * Parses a data reduction proxy bypass event for use in the summary and
     * in the event tbody.
     */
    parseBypassEvent_: function(event) {
      var reason;
      if (event.params.action != null) {
        reason = event.params.action;
      } else {
        reason = getKeyWithValue(
            DataReductionProxyBypassEventType, event.params.bypass_type);
      }

      var parsedBypass = {
        bypass_reason: reason,
        origin_url: event.params.url,
        bypass_duration_seconds: event.params.bypass_duration_seconds,
        bypass_expiration: event.params.expiration,
      };

      return parsedBypass;
    },

    /**
     * Updates the data reduction proxy summary block.
     */
    updateDataReductionProxyConfig_: function() {
      $(BandwidthView.PRIMARY_PROXY_ID).innerHTML = '';
      $(BandwidthView.SECONDARY_PROXY_ID).innerHTML = '';
      setNodeDisplay($(BandwidthView.BYPASS_STATE_CONTAINER_ID), false);

      if (this.data_reduction_proxy_config_) {
        var primaryProxy = '';
        var secondaryProxy = '';
        var hasBypassedProxy = false;
        var now = timeutil.getCurrentTimeTicks();

        if (this.last_bypass_ &&
            this.hasTimePassedLogTime_(+this.last_bypass_.bypass_expiration)) {
          var input = new JsEvalContext(this.last_bypass_);
          jstProcess(input, $(BandwidthView.BYPASS_STATE_CONTAINER_ID));
        } else {
          var input = new JsEvalContext();
          jstProcess(input, $(BandwidthView.BYPASS_STATE_CONTAINER_ID));
        }

        if (this.data_reduction_proxy_config_.ssl_origin) {
          if (this.isMarkedAsBad_(this.data_reduction_proxy_config_.ssl_origin))
            hasBypassedProxy = true;

          primaryProxy = 'HTTPS Tunnel: ' + this.buildProxyString_(
              this.data_reduction_proxy_config_.ssl_origin, false);
        }

        if (this.data_reduction_proxy_config_.primary_origin) {
          if (this.isMarkedAsBad_(
              this.data_reduction_proxy_config_.primary_origin))
            hasBypassedProxy = true;

          var proxyString = this.buildProxyString_(
              this.data_reduction_proxy_config_.primary_origin,
              this.data_reduction_proxy_config_.primary_restricted);

          if (primaryProxy == '')
            primaryProxy = proxyString;
          else
            secondaryProxy = proxyString;
        }

        if (this.data_reduction_proxy_config_.fallback_origin) {
          if (this.isMarkedAsBad_(
              this.data_reduction_proxy_config_.fallback_origin))
            hasBypassedProxy = true;

          var proxyString = this.buildProxyString_(
              this.data_reduction_proxy_config_.fallback_origin,
              this.data_reduction_proxy_config_.fallback_restricted);

          if (primaryProxy == '')
            primaryProxy = proxyString;
          else if (secondaryProxy == '')
            secondaryProxy = proxyString;
        }

        $(BandwidthView.PRIMARY_PROXY_ID).innerText = primaryProxy;
        $(BandwidthView.SECONDARY_PROXY_ID).innerText = secondaryProxy;
        if (hasBypassedProxy)
          setNodeDisplay($(BandwidthView.BYPASS_STATE_CONTAINER_ID), true);
      }
    },

    /**
     * Takes a data reduction proxy configuration and renders to a friendly
     * string.
     */
    buildProxyString_: function(proxy, restricted) {
      var markedAsBad = this.isMarkedAsBad_(proxy);
      var proxyString = '';
      if (restricted) {
        proxyString += proxy + ' (RESTRICTED)';
      } else {
        proxyString += proxy;
        if (markedAsBad) {
          proxyString += ' (BYPASSED)';
        } else {
          proxyString += ' (ACTIVE)';
        }
      }

      return proxyString;
    },

    /**
     * Checks to see if a proxy server is in marked as bad.
     */
    isMarkedAsBad_: function(proxy) {
      for (var entry in this.bad_proxy_config_) {
        if (entry == proxy &&
            this.hasTimePassedLogTime_(this.bad_proxy_config_[entry])) {
          return true;
        }
      }

      return false;
    },

    /**
     * Checks to see if a given time in ticks has passed the time of the
     * the log. For real time viewing, this is "now", but for loaded logs, it
     * is the time at which the logs were taken.
     */
    hasTimePassedLogTime_: function(timeTicks) {
      var logTime;
      if (MainView.isViewingLoadedLog() && ClientInfo.numericDate) {
        logTime = ClientInfo.numericDate;
      } else {
        logTime = timeutil.getCurrentTime();
      }

      return timeutil.convertTimeTicksToTime(timeTicks) > logTime;
    }
  };

  /**
   * Converts bytes to kilobytes rounded to one decimal place.
   */
  function bytesToRoundedKilobytes_(val) {
    return (val / 1024).toFixed(1);
  }

  /**
   * Returns bandwidth savings as a percent rounded to one decimal place.
   */
  function getPercentSavings_(original, received) {
    if (original > 0) {
      return ((original - received) * 100 / original).toFixed(1);
    }
    return '0.0';
  }

  return BandwidthView;
})();
