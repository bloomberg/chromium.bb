// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="stats_graph_helper.js"/>

var peerConnectionsListElem = null;

function initialize() {
  peerConnectionsListElem = $('peer-connections-list');
  chrome.send('getAllUpdates');
  startGetStats();
}

// Polls stats from all PeerConnections every second.
function startGetStats() {
  if (document.getElementsByTagName('li').length)
    chrome.send('getAllStats');
  window.setTimeout(startGetStats, 1000);
}

function getPeerConnectionId(data) {
  return data.pid + '-' + data.lid;
}

// Makes sure a LI element representing a PeerConnection is created
// and appended to peerConnectionListElem.
function ensurePeerConnectionElement(id) {
  var element = $(id);
  if (!element) {
    element = document.createElement('li');
    peerConnectionsListElem.appendChild(element);
    element.id = id;
  }
  return element;
}

// Makes sure the table representing the PeerConnection event log is created
// and appended to peerConnectionElement.
function ensurePeerConnectionLog(peerConnectionElement) {
  var logId = peerConnectionElement.id + '-log';
  var logElement = $(logId);
  if (!logElement) {
    var container = document.createElement('div');
    container.className = 'log-container';
    peerConnectionElement.appendChild(container);

    logElement = document.createElement('table');
    logElement.id = logId;
    logElement.className = 'log-table';
    logElement.border = 1;
    container.appendChild(logElement);
    logElement.innerHTML = '<tr><th>Time</th>' +
                           '<th class="log-header-event">Event</th></tr>';
  }
  return logElement;
}

// Add the update to the log table as a new row. The type of the update is set
// as the text of the cell; clicking the cell will reveal or hide the details
// as the content of a TextArea element.
function addToPeerConnectionLog(logElement, update) {
  var row = document.createElement('tr');
  logElement.appendChild(row);

  var expandable = (update.value.length > 0);
  // details.open is true initially so that we can get the real scrollHeight
  // of the textareas.
  row.innerHTML =
      '<td>' + (new Date()).toLocaleString() + '</td>' +
      (expandable ?
          '<td><details open><summary>' + update.type + '</summary>' +
              '</details></td>' :
          '<td>' + update.type + '</td>');
  if (!expandable)
    return;

  var valueContainer = document.createElement('textarea');
  var details = row.cells[1].childNodes[0];
  details.appendChild(valueContainer);
  valueContainer.value = update.value;
  valueContainer.style.height = valueContainer.scrollHeight + 'px';
  details.open = false;
}

// Ensure the DIV container for the stats tables is created as a child of
// |peerConnectionElement|.
function ensureStatsTableContainer(peerConnectionElement) {
  var containerId = peerConnectionElement.id + '-table-container';
  var container = $(containerId);
  if (!container) {
    container = document.createElement('div');
    container.id = containerId;
    container.className = 'stats-table-container';
    peerConnectionElement.appendChild(container);
  }
  return container;
}

// Ensure the stats table for track |statsId| of PeerConnection
// |peerConnectionElement| is created as a child of the stats table container.
function ensureStatsTable(peerConnectionElement, statsId) {
  var tableId = peerConnectionElement.id + '-table-' + statsId;
  var table = $(tableId);
  if (!table) {
    var container = ensureStatsTableContainer(peerConnectionElement);
    table = document.createElement('table');
    container.appendChild(table);
    table.id = tableId;
    table.border = 1;
    table.innerHTML = '<th>Statistics ' + statsId + '</th>';
  }
  return table;
}

// Update the value column of the stats row of |rowName| to |value|.
// A new row is created is this is the first report of this stats.
function updateStatsTableRow(statsTable, rowName, value) {
  var trId = statsTable.id + '-' + rowName;
  var trElement = $(trId);
  if (!trElement) {
    trElement = document.createElement('tr');
    trElement.id = trId;
    statsTable.appendChild(trElement);
    trElement.innerHTML = '<td>' + rowName + '</td><td></td>';
  }
  trElement.cells[1].textContent = value;
}

// Add |singleReport| to the stats table.
function addSingleReportToTable(statsTable, singleReport) {
  if (!singleReport || !singleReport.values || singleReport.values.length == 0)
    return;

  var date = Date(singleReport.timestamp);
  updateStatsTableRow(statsTable, 'timestamp', date.toLocaleString());
  for (var i = 0; i < singleReport.values.length - 1; i = i + 2) {
    updateStatsTableRow(statsTable, singleReport.values[i],
                        singleReport.values[i + 1]);
  }
}

//
// Browser message handlers.
//

// data = {pid:|integer|, lid:|integer|}.
function removePeerConnection(data) {
  var element = $(getPeerConnectionId(data));
  if (element)
    peerConnectionsListElem.removeChild(element);
}

// data = {pid:|integer|, lid:|integer|,
//         url:|string|, servers:|string|, constraints:|string|}.
function addPeerConnection(data) {
  var peerConnectionElement = ensurePeerConnectionElement(
      getPeerConnectionId(data));
  peerConnectionElement.innerHTML =
      '<h3>PeerConnection ' + peerConnectionElement.id + '</h3>' +
      '<div>' + data.url + ' ' + data.servers + ' ' + data.constraints +
      '</div>';
  return peerConnectionElement;
}

// data = {pid:|integer|, lid:|integer|, type:|string|, value:|string|}.
function updatePeerConnection(data) {
  var peerConnectionElement = ensurePeerConnectionElement(
      getPeerConnectionId(data));
  var logElement = ensurePeerConnectionLog(peerConnectionElement);
  addToPeerConnectionLog(logElement, data);
}

// data is an array and each entry is
// {pid:|integer|, lid:|integer|,
//  url:|string|, servers:|string|, constraints:|string|, log:|array|},
// each entry of log is {type:|string|, value:|string|}.
function updateAllPeerConnections(data) {
  for (var i = 0; i < data.length; ++i) {
    var peerConnection = addPeerConnection(data[i]);
    var logElement = ensurePeerConnectionLog(peerConnection);

    var log = data[i].log;
    for (var j = 0; j < log.length; ++j) {
      addToPeerConnectionLog(logElement, log[j]);
    }
  }
}

// data = {pid:|integer|, lid:|integer|, reports:|array|}.
// Each entry of reports =
// {id:|string|, type:|string|, local:|object|, remote:|object|}.
// reports.local or reports.remote =
// {timestamp: |double|, values: |array|},
// where values is an array of strings, whose even index entry represents
// the name of the stat, and the odd index entry represents the value of
// the stat.
function addStats(data) {
  var peerConnectionElement = ensurePeerConnectionElement(
      getPeerConnectionId(data));
  for (var i = 0; i < data.reports.length; ++i) {
    var report = data.reports[i];
    var reportName = report.type + '-' + report.id;
    var statsTable = ensureStatsTable(peerConnectionElement, reportName);

    addSingleReportToTable(statsTable, report.local);
    drawSingleReport(peerConnectionElement, reportName, report.local);
    addSingleReportToTable(statsTable, report.remote);
    drawSingleReport(peerConnectionElement, reportName, report.remote);
  }
}

document.addEventListener('DOMContentLoaded', initialize);
