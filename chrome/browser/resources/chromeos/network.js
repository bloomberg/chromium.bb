// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var NetworkUI = function() {
  // Properties to display in the network state table
  var NETWORK_STATE_FIELDS = [
    'Name', 'Type', 'State', 'Profile', 'Connectable',
    'Error', 'Address', 'Security', 'Cellular.NetworkTechnology',
    'Cellular.ActivationState', 'Cellular.RoamingState',
    'Cellular.OutOfCredits', 'Strength'
  ];

  var LOG_LEVEL_CLASSNAME = {
    'Error': 'network-log-level-error',
    'User': 'network-log-level-user',
    'Event': 'network-log-level-event',
    'Debug': 'network-log-level-debug'
  };

  var LOG_LEVEL_CHECKBOX = {
    'Error': 'log-error',
    'User': 'log-user',
    'Event': 'log-event',
    'Debug': 'log-debug'
  };

  /**
   * Create a tag of log level.
   *
   * @param {string} level A string that represents log level.
   * @return {DOMElement} The created span element.
   */
  var createLevelTag = function(level) {
    var tag = document.createElement('span');
    tag.className = 'network-level-tag';
    tag.textContent = level;
    tag.classList.add(LOG_LEVEL_CLASSNAME[level]);
    return tag;
  };

  /**
   * Creates an element that contains the time, the event, the level and
   * the description of the given log entry.
   *
   * @param {Object} logEntry An object that represents a single line of log.
   * @return {DOMElement}  The created p element that represents the log entry.
   */
  var createLogEntryText = function(logEntry) {
    var level = logEntry['level'];
    if (!$(LOG_LEVEL_CHECKBOX[level]).checked)
      return null;
    var res = document.createElement('p');
    var textWrapper = document.createElement('span');
    var fileinfo = '';
    if ($('log-fileinfo').checked)
      fileinfo = logEntry['file'];
    textWrapper.textContent = loadTimeData.getStringF(
      'logEntryFormat',
      logEntry['timestamp'],
      fileinfo,
      logEntry['event'],
      logEntry['description']);
    res.appendChild(createLevelTag(level));
    res.appendChild(textWrapper);
    return res;
  };

  /**
   * Create event log entries.
   *
   * @param {Array.<string>} logEntries A array of strings that each string
   *     represents a log event in JSON format.
   */
  var createEventLog = function(logEntries) {
    var container = $('network-log-container');
    container.textContent = '';
    for (var i = 0; i < logEntries.length; ++i) {
      var entry = createLogEntryText(JSON.parse(logEntries[i]));
      if (entry)
        container.appendChild(entry);
    }
  };

  /**
   * Create a cell in network state table.
   *
   * @param {string} value Content in the cell.
   * @return {DOMElement} The created td element that displays the given value.
   */
  var createStatusTableCell = function(value) {
    var col = document.createElement('td');
    col.textContent = value || '';
    return col;
  };

  /**
   * Create a row in the network state table.
   *
   * @param {string} path The network path.
   * @param {dictionary} status Properties of the network.
   * @return {DOMElement} The created tr element that contains the network
   *     state information.
   */
  var createStatusTableRow = function(path, status) {
    var row = document.createElement('tr');
    row.className = 'network-status-table-row';
    row.appendChild(createStatusTableCell(path));
    row.appendChild(createStatusTableCell(status['GUID'].slice(1, 9)));
    for (var i = 0; i < NETWORK_STATE_FIELDS.length; ++i) {
      row.appendChild(createStatusTableCell(status[NETWORK_STATE_FIELDS[i]]));
    }
    return row;
  };

  /**
   * Create network state table.
   *
   * @param {Array.<Object>} networkStatuses An array of network states.
   */
  var createNetworkTable = function(networkStatuses) {
    var table = $('network-status-table');
    var oldRows = table.querySelectorAll('.network-status-table-row');
    for (var i = 0; i < oldRows.length; ++i)
      table.removeChild(oldRows[i]);
    for (var path in networkStatuses)
      table.appendChild(
          createStatusTableRow(path, networkStatuses[path]));
  };

  /**
   * This callback function is triggered when the data is received.
   *
   * @param {dictionary} data A dictionary that contains network state
   *     information.
   */
  var onNetworkInfoReceived = function(data) {
    createEventLog(JSON.parse(data.networkEventLog));
    createNetworkTable(data.networkState);
  };

  /**
   * Sends a refresh request.
   */
  var sendRefresh = function() {
    chrome.send('requestNetworkInfo');
  }

  /**
   * Sets refresh rate if the interval is found in the url.
   */
  var setRefresh = function() {
    var interval = parseQueryParams(window.location)['refresh'];
    if (interval && interval != '')
      setInterval(sendRefresh, parseInt(interval) * 1000);
  };

  /**
   * Get network information from WebUI.
   */
  document.addEventListener('DOMContentLoaded', function() {
    $('log-refresh').onclick = sendRefresh;
    $('log-error').checked = true;
    $('log-error').onclick = sendRefresh;
    $('log-user').checked = true;
    $('log-user').onclick = sendRefresh;
    $('log-event').checked = true;
    $('log-event').onclick = sendRefresh;
    $('log-debug').checked = false;
    $('log-debug').onclick = sendRefresh;
    $('log-fileinfo').checked = false;
    $('log-fileinfo').onclick = sendRefresh;
    setRefresh();
    sendRefresh();
  });

  return {
    onNetworkInfoReceived: onNetworkInfoReceived
  };
}();
