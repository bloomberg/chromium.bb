// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var NetworkUI = (function() {
  'use strict';

  // Properties to display in the network state table. Each entry can be either
  // a single state field or an array of state fields. If more than one is
  // specified then the first non empty value is used.
  var NETWORK_STATE_FIELDS = [
    'GUID',
    'service_path',
    'Name',
    'Type',
    'ConnectionState',
    'connectable',
    'ErrorState',
    'WiFi.Security',
    ['Cellular.NetworkTechnology',
     'EAP.EAP'],
    'Cellular.ActivationState',
    'Cellular.RoamingState',
    'Cellular.OutOfCredits',
    'WiFi.SignalStrength'
  ];

  var FAVORITE_STATE_FIELDS = [
    'GUID',
    'service_path',
    'Name',
    'Type',
    'profile_path',
    'visible',
    'Source'
  ];

  /**
   * Returns the property associated with a key (which may reference a
   * sub-object).
   *
   * @param {Object} properties The object containing the network properties.
   * @param {string} key The ONC key for the property. May refer to a nested
   *     propety, e.g. 'WiFi.Security'.
   * @return {*} The value associated with the property.
   */
  var getValueFromProperties = function(properties, key) {
    if (!key) {
      console.error('Empty key');
      return undefined;
    }
    var dot = key.indexOf('.');
    if (dot > 0) {
      var key1 = key.substring(0, dot);
      var key2 = key.substring(dot + 1);
      var subobject = properties[key1];
      if (subobject)
        return getValueFromProperties(subobject, key2);
    }
    return properties[key];
  };

  /**
   * Creates a cell with a button for expanding a network state table row.
   *
   * @param {string} guid The GUID identifying the network.
   * @return {!HTMLElement} The created td element that displays the given
   *     value.
   */
  var createStateTableExpandButton = function(guid) {
    var cell = /** @type {!HTMLElement} */ (document.createElement('td'));
    cell.className = 'state-table-expand-button-cell';
    var button = document.createElement('button');
    button.addEventListener('click', function(event) {
      toggleExpandRow(/** @type {!HTMLElement} */ (event.target), guid);
    });
    button.className = 'state-table-expand-button';
    button.textContent = '+';
    cell.appendChild(button);
    return cell;
  };

  /**
   * Creates a cell in network state table.
   *
   * @param {string} value Content in the cell.
   * @return {!HTMLElement} The created td element that displays the given
   *     value.
   */
  var createStateTableCell = function(value) {
    var cell = /** @type {!HTMLElement} */ (document.createElement('td'));
    cell.textContent = value || '';
    return cell;
  };

  /**
   * Creates a row in the network state table.
   *
   * @param {Array} stateFields The state fields to use for the row.
   * @param {Object} state Property values for the network or favorite.
   * @return {!HTMLElement} The created tr element that contains the network
   *     state information.
   */
  var createStateTableRow = function(stateFields, state) {
    var row = /** @type {!HTMLElement} */ (document.createElement('tr'));
    row.className = 'state-table-row';
    var guid = state['GUID'];
    row.appendChild(createStateTableExpandButton(guid));
    for (var i = 0; i < stateFields.length; ++i) {
      var field = stateFields[i];
      var value = '';
      if (typeof field == 'string') {
        value = getValueFromProperties(state, field);
      } else {
        for (var j = 0; j < field.length; ++j) {
          value = getValueFromProperties(state, field[j]);
          if (value)
            break;
        }
      }
      if (field == 'GUID')
        value = value.slice(0, 8);
      row.appendChild(createStateTableCell(/** @type {string} */ (value)));
    }
    return row;
  };

  /**
   * Creates a table for networks or favorites.
   *
   * @param {string} tablename The name of the table to be created.
   * @param {Array} stateFields The list of fields for the table.
   * @param {Array} states An array of network or favorite states.
   */
  var createStateTable = function(tablename, stateFields, states) {
    var table = $(tablename);
    var oldRows = table.querySelectorAll('.state-table-row');
    for (var i = 0; i < oldRows.length; ++i)
      table.removeChild(oldRows[i]);
    states.forEach(function(state) {
      table.appendChild(createStateTableRow(stateFields, state));
    });
  };

  /**
   * Returns a valid HTMLElement id from |guid|.
   *
   * @param {string} guid A GUID which may start with a digit
   * @return {string} A valid HTMLElement id.
   */
  var idFromGuid = function(guid) {
    return '_' + guid.replace(/[{}]/g, '');
  };

  /**
   * This callback function is triggered when visible networks are received.
   *
   * @param {Array} states A list of network state information for each
   *     visible network.
   */
  var onVisibleNetworksReceived = function(states) {
    createStateTable('network-state-table', NETWORK_STATE_FIELDS, states);
  };

  /**
   * This callback function is triggered when favorite networks are received.
   *
   * @param {!Array} states A list of network state information for each
   *     favorite network.
   */
  var onFavoriteNetworksReceived = function(states) {
    createStateTable('favorite-state-table', FAVORITE_STATE_FIELDS, states);
  };

  /**
   * Toggles the button state and add or remove a row displaying the complete
   * state information for a row.
   *
   * @param {!HTMLElement} btn The button that was clicked.
   * @param {string} guid GUID identifying the network.
   */
  var toggleExpandRow = function(btn, guid) {
    var cell = btn.parentNode;
    var row = /** @type {!HTMLElement} */ (cell.parentNode);
    if (btn.textContent == '-') {
      btn.textContent = '+';
      row.parentNode.removeChild(row.nextSibling);
    } else {
      btn.textContent = '-';
      var expandedRow = createExpandedRow(guid, row);
      row.parentNode.insertBefore(expandedRow, row.nextSibling);
    }
  };

  /**
   * Creates the expanded row for displaying the complete state as JSON.
   *
   * @param {string} guid The GUID identifying the network.
   * @param {!HTMLElement} baseRow The unexpanded row associated with the new
   *     row.
   * @return {!HTMLElement} The created tr element for the expanded row.
   */
  var createExpandedRow = function(guid, baseRow) {
    var expandedRow = /** @type {!HTMLElement} */ (
      document.createElement('tr'));
    expandedRow.className = 'state-table-row';
    var emptyCell = document.createElement('td');
    emptyCell.style.border = 'none';
    expandedRow.appendChild(emptyCell);
    var detailCell = document.createElement('td');
    detailCell.id = idFromGuid(guid);
    detailCell.className = 'state-table-expanded-cell';
    detailCell.colSpan = baseRow.childNodes.length - 1;
    expandedRow.appendChild(detailCell);
    var showDetail = function(state, error) {
      if (error && error.message)
        detailCell.textContent = error.message;
      else
        detailCell.textContent = JSON.stringify(state, null, '\t');
    };
    var selected = $('get-property-format').selectedIndex;
    var selectedId = $('get-property-format').options[selected].value;
    if (selectedId == 'shill') {
      chrome.send('getShillProperties', [guid]);
    } else if (selectedId == 'managed') {
      chrome.networkingPrivate.getManagedProperties(guid, function(properties) {
        showDetail(properties, chrome.runtime.lastError); });
    } else {
      chrome.networkingPrivate.getProperties(guid, function(properties) {
        showDetail(properties, chrome.runtime.lastError); });
    }
    return expandedRow;
  };

  /**
   * Callback invoked by Chrome after a getShillProperties call.
   *
   * @param {Array} args The requested Shill properties. Will contain
   *     just the 'GUID' and 'ShillError' properties if the call failed.
   */
  var getShillPropertiesResult = function(args) {
    var properties = args.shift();
    var guid = properties['GUID'];
    if (!guid) {
      console.error('No GUID in getShillPropertiesResult');
      return;
    }

    var detailCell = document.querySelector('td#' + idFromGuid(guid));
    if (!detailCell) {
      console.error('No cell for GUID: ' + guid);
      return;
    }

    if (properties['ShillError'])
      detailCell.textContent = properties['ShillError'];
    else
      detailCell.textContent = JSON.stringify(properties, null, '\t');

  };

  /**
   * Requests an update of all network info.
   */
  var requestNetworks = function() {
    chrome.networkingPrivate.getNetworks(
        {'networkType': 'All', 'visible': true}, onVisibleNetworksReceived);
    chrome.networkingPrivate.getNetworks(
        {'networkType': 'All', 'configured': true}, onFavoriteNetworksReceived);
  };

  /**
   * Sets refresh rate if the interval is found in the url.
   */
  var setRefresh = function() {
    var interval = parseQueryParams(window.location)['refresh'];
    if (interval && interval != '')
      setInterval(requestNetworks, parseInt(interval, 10) * 1000);
  };

  /**
   * Gets network information from WebUI.
   */
  document.addEventListener('DOMContentLoaded', function() {
    $('refresh').onclick = requestNetworks;
    setRefresh();
    requestNetworks();
  });

  return {
    getShillPropertiesResult: getShillPropertiesResult
  };
})();
