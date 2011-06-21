// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information related to Prerendering.
 * @constructor
 */
function PrerenderView(mainBoxId, prerenderEnabledSpanId, prerenderHistoryDivId,
                       prerenderActiveDivId) {
  DivView.call(this, mainBoxId);
  g_browser.addPrerenderInfoObserver(this);
  this.prerenderEnabledSpan_ = document.getElementById(prerenderEnabledSpanId);
  this.prerenderHistoryDiv_ = document.getElementById(prerenderHistoryDivId);
  this.prerenderActiveDiv_ = document.getElementById(prerenderActiveDivId);
}

inherits(PrerenderView, DivView);

function IsValidPrerenderInfo(prerenderInfo) {
  if (prerenderInfo == null) {
    return false;
  }
  if (!('history' in prerenderInfo) ||
      !('active' in prerenderInfo) ||
      !('enabled' in prerenderInfo)) {
    return false;
  }
  return true;
}

PrerenderView.prototype.onPrerenderInfoChanged = function(prerenderInfo) {
  this.prerenderEnabledSpan_.innerText = '';
  this.prerenderHistoryDiv_.innerHTML = '';
  this.prerenderActiveDiv_.innerHTML = '';
  if (!IsValidPrerenderInfo(prerenderInfo)) {
    return;
  }

  this.prerenderEnabledSpan_.innerText = prerenderInfo.enabled.toString();

  var tabPrinter = PrerenderView.createHistoryTablePrinter(
      prerenderInfo.history);
  tabPrinter.toHTML(this.prerenderHistoryDiv_, 'styledTable');

  var tabPrinter = PrerenderView.createActiveTablePrinter(
      prerenderInfo.active);
  tabPrinter.toHTML(this.prerenderActiveDiv_, 'styledTable');
};

PrerenderView.createHistoryTablePrinter = function(prerenderHistory) {
  var tablePrinter = new TablePrinter();
  tablePrinter.addHeaderCell('URL');
  tablePrinter.addHeaderCell('Final Status');
  tablePrinter.addHeaderCell('Time');

  for (var i = 0; i < prerenderHistory.length; i++) {
    var historyEntry = prerenderHistory[i];
    tablePrinter.addRow();
    tablePrinter.addCell(historyEntry.url);
    tablePrinter.addCell(historyEntry.final_status);

    var date = new Date(parseInt(historyEntry.end_time));
    tablePrinter.addCell(date.toLocaleString());
  }
  return tablePrinter;
};

PrerenderView.createActiveTablePrinter = function(prerenderActive) {
  var tablePrinter = new TablePrinter();
  tablePrinter.addHeaderCell('URL');
  tablePrinter.addHeaderCell('Duration');

  for (var i = 0; i < prerenderActive.length; i++) {
    var activeEntry = prerenderActive[i];
    tablePrinter.addRow();
    tablePrinter.addCell(activeEntry.url);
    tablePrinter.addCell(activeEntry.duration);
  }
  return tablePrinter;
};

