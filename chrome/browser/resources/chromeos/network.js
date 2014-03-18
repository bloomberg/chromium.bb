// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var NetworkUI = function() {
  // Properties to display in the network state table. Each entry can be either
  // a single state field or an array of state fields. If more than one is
  // specified then the first non empty value is used.
  var NETWORK_STATE_FIELDS = [
    'Name', 'Type', 'State', 'Profile', 'Connectable',
    'Error', 'Address', 'Security',
    ['Cellular.NetworkTechnology', 'EAP.EAP'],
    'Cellular.ActivationState', 'Cellular.RoamingState',
    'Cellular.OutOfCredits', 'Strength'
  ];

  var FAVORITE_STATE_FIELDS = [
    'Name', 'Type', 'Profile', 'onc_source'
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
    var timestamp = '';
    if ($('log-timedetail').checked)
      timestamp = logEntry['timestamp'];
    else
      timestamp = logEntry['timestampshort'];
    textWrapper.textContent = loadTimeData.getStringF(
      'logEntryFormat',
      timestamp,
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
  var createStateTableCell = function(value) {
    var col = document.createElement('td');
    col.textContent = value || '';
    return col;
  };

  /**
   * Create a row in the network state table.
   *
   * @param {string} stateFields The state fields to use for the row.
   * @param {string} path The network or favorite path.
   * @param {dictionary} state Property values for the network or favorite.
   * @return {DOMElement} The created tr element that contains the network
   *     state information.
   */
  var createStateTableRow = function(stateFields, path, state) {
    var row = document.createElement('tr');
    row.className = 'state-table-row';
    row.appendChild(createStateTableCell(path));
    row.appendChild(createStateTableCell(state['GUID'].slice(1, 9)));
    for (var i = 0; i < stateFields.length; ++i) {
      var field = stateFields[i];
      var value = '';
      if (typeof field == 'string') {
        value = state[field];
      } else {
        for (var j = 0; j < field.length; ++j) {
          value = state[field[j]];
          if (value)
            break;
        }
      }
      row.appendChild(createStateTableCell(value));
    }
    return row;
  };

  /**
   * Create table for networks or favorites.
   *
   * @param {string} tablename The name of the table to be created.
   * @param {Array.<Object>} stateFields The list of fields for the table.
   * @param {Array.<Object>} states An array of network or favorite states.
   */
  var createStateTable = function(tablename, stateFields, states) {
    var table = $(tablename);
    var oldRows = table.querySelectorAll('.state-table-row');
    for (var i = 0; i < oldRows.length; ++i)
      table.removeChild(oldRows[i]);
    for (var path in states)
      table.appendChild(createStateTableRow(stateFields, path, states[path]));
  };

  /**
   * This callback function is triggered when the data is received.
   *
   * @param {dictionary} data A dictionary that contains network state
   *     information.
   */
  var onNetworkInfoReceived = function(data) {
    createEventLog(JSON.parse(data.networkEventLog));
    createStateTable(
        'network-state-table', NETWORK_STATE_FIELDS, data.networkStates);
    createStateTable(
        'favorite-state-table', FAVORITE_STATE_FIELDS, data.favoriteStates);
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
    $('log-timedetail').checked = false;
    $('log-timedetail').onclick = sendRefresh;
    setRefresh();
    sendRefresh();
  });

  return {
    onNetworkInfoReceived: onNetworkInfoReceived
  };
}();
