// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays network related log data and is specific fo ChromeOS.
 * We get log data from chrome by filtering system logs for network related
 * keywords. Logs are not fetched until we actually need them.
 *
 * @constructor
 */
function LogsView() {
  const mainBoxId = 'logsTabContent';
  const tableId = 'logTable';
  const globalShowButtonId = 'logsGlobalShowBtn';
  const globalHideButtonId = 'logsGlobalHideBtn';
  const refreshLogsButtonId = 'logsRefreshBtn';

  var tableDiv = $(tableId);
  this.rows = [];
  this.PopulateTable(tableDiv, this.logFilterList);
  $(globalShowButtonId).addEventListener('click',
      this.onGlobalChangeVisibleClick_.bind(this, true));
  $(globalHideButtonId).addEventListener('click',
      this.onGlobalChangeVisibleClick_.bind(this, false));
  $(refreshLogsButtonId).addEventListener('click',
      this.onLogsRefresh_.bind(this));
  DivView.call(this, mainBoxId);
};

inherits(LogsView, DivView);

/**
 * Contains log keys we are interested in.
 */
LogsView.prototype.logFilterList = [
  {
    key:'syslog',
  },
  {
    key:'ui_log',
  },
  {
    key:'chrome_system_log',
  },
  {
    key:'chrome_log',
  },
];

/**
 * Called during View's initialization. Creates the row of a table logs will be
 * shown in. Each row has 4 cells.
 *
 * First cell's content will be set to |logKey|, second will contain a button
 * that will be used to show or hide third cell, which will contain the filtered
 * log.
 * |logKey| also tells us which log we are getting data from.
 */
LogsView.prototype.CreateTableRow = function(logKey) {
  var row = document.createElement('tr');

  var cells = [];
  for (var i = 0; i < 3; i++) {
    var rowCell = document.createElement('td');
    cells.push(rowCell);
    row.appendChild(rowCell);
  }
  // Log key cell.
  cells[0].className = 'logCellText';
  cells[0].textContent = logKey;
  // Cell log is displayed in. Log content is in div element that is initially
  // hidden and empty.
  cells[2].className = 'logCellText';
  var logDiv = document.createElement('div');
  logDiv.textContent = '';
  logDiv.className = 'logCellLog';
  logDiv.id = 'logsView.logCell.' + this.rows.length;
  cells[2].appendChild(logDiv);

  // Button that we use to show or hide div element with log content. Logs are
  // not visible initially, so we initialize button accordingly.
  var expandButton = document.createElement('button');
  expandButton.textContent = 'Show...';
  expandButton.className = 'logButton';
  expandButton.addEventListener('click',
                                this.onButtonClicked_.bind(this, row));

  // Cell that contains show/hide button.
  cells[1].appendChild(expandButton);
  cells[1].className = 'logTableButtonColumn';

  // Initially, log is not visible.
  row.className = 'logRowCollapsed';

  // We will need those to process row buttons' onclick events.
  row.logKey = logKey;
  row.expandButton = expandButton;
  row.logDiv = logDiv;
  row.logVisible = false;
  this.rows.push(row);

  return row;
};

/**
 * Initializes |tableDiv| to represent data from |logList| which should be of
 * type LogsView.logFilterList.
 */
LogsView.prototype.PopulateTable = function(tableDiv, logList) {
  for (var i = 0; i < logList.length; i++) {
    var logSource = this.CreateTableRow(logList[i].key);
    tableDiv.appendChild(logSource);
  }
};

/**
 * Processes clicks on buttons that show or hide log contents in log row.
 * Row containing the clicked button is given to the method since it contains
 * all data we need to process the click (unlike button object itself).
 */
LogsView.prototype.onButtonClicked_ = function(containingRow) {
  if (!containingRow.logVisible) {
    containingRow.className = 'logRowExpanded';
    containingRow.expandButton.textContent = 'Hide...';
    var logDiv = containingRow.logDiv;
    if (logDiv.textContent == '') {
      logDiv.textContent = 'Getting logs...';
      // Callback will be executed by g_browser.
      g_browser.getSystemLog(containingRow.logKey,
                             containingRow.logDiv.id);
    }
  } else {
    containingRow.className = 'logRowCollapsed';
    containingRow.expandButton.textContent = 'Show...';
  }
  containingRow.logVisible = !containingRow.logVisible;
};

/**
 * Processes click on one of the buttons that are used to show or hide all logs
 * we care about.
 */
LogsView.prototype.onGlobalChangeVisibleClick_ = function(isShowAll) {
  for (var row in this.rows) {
    if (isShowAll != this.rows[row].logVisible) {
      this.onButtonClicked_(this.rows[row]);
    }
  }
};

/**
 * Processes click event on the button we use to refresh fetched logs. we get
 * the newest logs from libcros, and refresh content of the visible log cells.
 */
LogsView.prototype.onLogsRefresh_ = function() {
  g_browser.refreshSystemLogs();

  var visibleLogRows = [];
  var hiddenLogRows = [];
  for (var row in this.rows) {
    if (this.rows[row].logVisible) {
      visibleLogRows.push(this.rows[row]);
    } else {
      hiddenLogRows.push(this.rows[row]);
    }
  }

  // We have to refresh text content in visible rows.
  for (row in visibleLogRows) {
    visibleLogRows[row].logDiv.textContent = 'Getting logs...';
    g_browser.getSystemLog(visibleLogRows[row].logKey,
                           visibleLogRows[row].logDiv.id);
  }

  // In hidden rows we just clear potential log text, so we know we have to get
  // new contents when we show the row next time.
  for (row in hiddenLogRows) {
    hiddenLogRows[row].logDiv.textContent = '';
  }
};
