// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays a summary of the current Reporting cache, including the
 * configuration headers received for Reporting-enabled origins, and any queued
 * reports that are waiting to be uploaded.
 */
var ReportingView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function ReportingView() {
    assertFirstConstructorCall(ReportingView);

    // Call superclass's constructor.
    superClass.call(this, ReportingView.MAIN_BOX_ID);

    g_browser.addReportingInfoObserver(this, true);
  }

  ReportingView.TAB_ID = 'tab-handle-reporting';
  ReportingView.TAB_NAME = 'Reporting';
  ReportingView.TAB_HASH = '#reporting';

  // IDs for special HTML elements in reporting_view.html
  ReportingView.MAIN_BOX_ID = 'reporting-view-tab-content';

  ReportingView.DISABLED_BOX_ID = 'reporting-view-disabled-content';
  ReportingView.ENABLED_BOX_ID = 'reporting-view-enabled-content';

  ReportingView.CLIENTS_EMPTY_ID = 'reporting-view-clients-empty';
  ReportingView.CLIENTS_TABLE_ID = 'reporting-view-clients-table';
  ReportingView.CLIENTS_TBODY_ID = 'reporting-view-clients-tbody';
  ReportingView.REPORTS_EMPTY_ID = 'reporting-view-reports-empty';
  ReportingView.REPORTS_TABLE_ID = 'reporting-view-reports-table';
  ReportingView.REPORTS_TBODY_ID = 'reporting-view-reports-tbody';

  cr.addSingletonGetter(ReportingView);

  ReportingView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onReportingInfoChanged(data.reportingInfo);
    },

    onReportingInfoChanged: function(reportingInfo) {
      if (!reportingInfo)
        return false;

      var enabled = reportingInfo.reportingEnabled;
      setNodeDisplay($(ReportingView.DISABLED_BOX_ID), !enabled);
      setNodeDisplay($(ReportingView.ENABLED_BOX_ID), enabled);

      displayReportDetail_(reportingInfo.reports);
      displayClientDetail_(reportingInfo.clients);
      return true;
    },
  };

  /**
   * Displays information about each queued report in the Reporting cache.
   */
  function displayReportDetail_(reportList) {
    // Clear the existing content.
    $(ReportingView.REPORTS_TBODY_ID).innerHTML = '';

    var empty = reportList.length == 0;
    setNodeDisplay($(ReportingView.REPORTS_EMPTY_ID), empty);
    setNodeDisplay($(ReportingView.REPORTS_TABLE_ID), !empty);
    if (empty)
      return;

    for (var i = 0; i < reportList.length; ++i) {
      var report = reportList[i];
      var tr = addNode($(ReportingView.REPORTS_TBODY_ID), 'tr');

      var queuedNode = addNode(tr, 'td');
      var queuedDate = timeutil.convertTimeTicksToDate(report.queued);
      timeutil.addNodeWithDate(queuedNode, queuedDate);

      addNodeWithText(tr, 'td', report.url);

      var statusNode = addNode(tr, 'td');
      addTextNode(statusNode, report.status + ' (' + report.group);
      if (report.depth > 0)
        addTextNode(statusNode, ', depth: ' + report.depth);
      if (report.attempts > 0)
        addTextNode(statusNode, ', attempts: ' + report.attempts);
      addTextNode(statusNode, ')');

      addNodeWithText(tr, 'td', report.type);

      var contentNode = addNode(tr, 'td');
      if (report.type == 'network-error')
        displayNetworkErrorContent_(contentNode, report);
      else
        displayGenericReportContent_(contentNode, report);
    }
  }

  /**
   * Adds nodes to the "content" cell for a report that allow you to show a
   * summary as well as collapsable detail.  We will add a clickable button that
   * toggles between showing and hiding the detail; its label will be `showText`
   * when the detail is hidden, and `hideText` when it's visible.
   *
   * The result is an object containing `summary` and `detail` nodes.  You can
   * add whatever content you want to each of these nodes.  The summary should
   * be a one-liner, and will be a <span>.  The detail can be as large as you
   * want, and will be a <div>.
   */
  function addContentSections_(contentNode, showText, hideText) {
    var sections = {};

    sections.summary = addNode(contentNode, 'span');
    sections.summary.classList.add('reporting-content-summary');

    var button = addNode(contentNode, 'span');
    button.classList.add('reporting-content-expand-button');
    addTextNode(button, showText);
    button.onclick = function() {
      toggleNodeDisplay(sections.detail);
      button.textContent =
          getNodeDisplay(sections.detail) ? hideText : showText;
    };

    sections.detail = addNode(contentNode, 'div');
    sections.detail.classList.add('reporting-content-detail');
    setNodeDisplay(sections.detail, false);

    return sections;
  }

  /**
   * Displays format-specific detail for Network Error Logging reports.
   */
  function displayNetworkErrorContent_(contentNode, report) {
    var contentSections =
        addContentSections_(contentNode, 'Show raw report', 'Hide raw report');

    addTextNode(contentSections.summary, report.body.type);
    // Only show the status code if it's present and not 0.
    if (report.body['status-code'])
      addTextNode(
          contentSections.summary, ' (' + report.body['status-code'] + ')');

    addNodeWithText(
        contentSections.detail, 'pre', JSON.stringify(report, null, '  '));
  }

  /**
   * Displays a generic content cell for reports whose type we don't know how to
   * render something specific for.
   */
  function displayGenericReportContent_(contentNode, report) {
    var contentSections =
        addContentSections_(contentNode, 'Show raw report', 'Hide raw report');
    addNodeWithText(
        contentSections.detail, 'pre', JSON.stringify(report, null, '  '));
  }

  /**
   * Displays information about each origin that has provided Reporting headers.
   */
  function displayClientDetail_(clientList) {
    // Clear the existing content.
    $(ReportingView.CLIENTS_TBODY_ID).innerHTML = '';

    var empty = clientList.length == 0;
    setNodeDisplay($(ReportingView.CLIENTS_EMPTY_ID), empty);
    setNodeDisplay($(ReportingView.CLIENTS_TABLE_ID), !empty);
    if (empty)
      return;

    for (var i = 0; i < clientList.length; ++i) {
      var now = new Date();

      var client = clientList[i];

      // Calculate the total number of endpoints for this origin, so that we can
      // rowspan its origin cell.
      var originHeight = 0;
      for (var j = 0; j < client.groups.length; ++j) {
        var group = client.groups[j];
        originHeight += group.endpoints.length;
      }

      for (var j = 0; j < client.groups.length; ++j) {
        var group = client.groups[j];
        for (var k = 0; k < group.endpoints.length; ++k) {
          var endpoint = group.endpoints[k];
          var tr = addNode($(ReportingView.CLIENTS_TBODY_ID), 'tr');

          if (j == 0 && k == 0) {
            var originNode = addNode(tr, 'td');
            originNode.setAttribute('rowspan', originHeight);
            addTextNode(originNode, client.origin);
          }

          if (k == 0) {
            var groupNode = addNode(tr, 'td');
            groupNode.setAttribute('rowspan', group.endpoints.length);
            addTextNode(groupNode, group.name);

            var expiresNode = addNode(tr, 'td');
            expiresNode.setAttribute('rowspan', group.endpoints.length);
            var expiresDate = timeutil.convertTimeTicksToDate(group.expires);
            timeutil.addNodeWithDate(expiresNode, expiresDate);
            if (now > expiresDate) {
              var expiredSpan = addNode(expiresNode, 'span');
              expiredSpan.classList.add('warning-text');
              addTextNode(expiredSpan, ' [expired]');
            }
          }

          var endpointNode = addNode(tr, 'td');
          addTextNode(endpointNode, endpoint.url);

          var priorityNode = addNode(tr, 'td');
          priorityNode.classList.add('reporting-centered');
          addTextNode(priorityNode, endpoint.priority);

          var weightNode = addNode(tr, 'td');
          weightNode.classList.add('reporting-centered');
          addTextNode(weightNode, endpoint.weight);

          addUploadCount_(tr, endpoint.successful);
          addUploadCount_(tr, endpoint.failed);
        }
      }
    }
  }

  /**
   * Adds an upload count cell to the client details table.
   */
  function addUploadCount_(tr, counts) {
    var node = addNode(tr, 'td');
    node.classList.add('reporting-centered');
    if (counts.uploads == 0 && counts.reports == 0) {
      addTextNode(node, '-');
    } else {
      addTextNode(node, counts.uploads + ' (' + counts.reports + ')');
    }
  }

  return ReportingView;
})();
