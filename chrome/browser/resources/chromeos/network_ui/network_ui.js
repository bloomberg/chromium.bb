// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var NetworkUI = (function() {
  'use strict';

  CrOncStrings = {
    OncTypeCellular: loadTimeData.getString('OncTypeCellular'),
    OncTypeEthernet: loadTimeData.getString('OncTypeEthernet'),
    OncTypeVPN: loadTimeData.getString('OncTypeVPN'),
    OncTypeWiFi: loadTimeData.getString('OncTypeWiFi'),
    OncTypeWiMAX: loadTimeData.getString('OncTypeWiMAX'),
    networkDisabled: loadTimeData.getString('networkDisabled'),
    networkListItemConnected:
        loadTimeData.getString('networkListItemConnected'),
    networkListItemConnecting:
        loadTimeData.getString('networkListItemConnecting'),
    networkListItemConnectingTo:
        loadTimeData.getString('networkListItemConnectingTo'),
    networkListItemNotConnected:
        loadTimeData.getString('networkListItemNotConnected'),
    vpnNameTemplate: loadTimeData.getString('vpnNameTemplate'),
  };

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
    ['Cellular.NetworkTechnology', 'EAP.EAP'],
    'Cellular.ActivationState',
    'Cellular.RoamingState',
    'WiFi.Frequency',
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
   * Creates and returns a typed HTMLTableCellElement.
   *
   * @return {!HTMLTableCellElement} A new td element.
   */
  var createTableCellElement = function() {
    return /** @type {!HTMLTableCellElement} */(document.createElement('td'));
  };

  /**
   * Creates and returns a typed HTMLTableRowElement.
   *
   * @return {!HTMLTableRowElement} A new tr element.
   */
  var createTableRowElement = function() {
    return /** @type {!HTMLTableRowElement} */(document.createElement('tr'));
  };

  /**
   * Returns the ONC data property for networkState associated with a key. Used
   * to access properties in the networkState by |key| which may may refer to a
   * nested property, e.g. 'WiFi.Security'. If any part of a nested key is
   * missing, this will return undefined.
   *
   * @param {!chrome.networkingPrivate.NetworkStateProperties} networkState The
   *     network state property dictionary.
   * @param {string} key The ONC key for the property.
   * @return {*} The value associated with the property or undefined if the
   *     key (any part of it) is not defined.
   */
  var getOncProperty = function(networkState, key) {
    var dict = /** @type {!Object} */(networkState);
    var keys = key.split('.');
    while (keys.length > 1) {
      var k = keys.shift();
      dict = dict[k];
      if (!dict || typeof dict != 'object')
        return undefined;
    }
    return dict[keys.shift()];
  };

  /**
   * Creates a cell with a button for expanding a network state table row.
   *
   * @param {string} guid The GUID identifying the network.
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     given value.
   */
  var createStateTableExpandButton = function(guid) {
    var cell = createTableCellElement();
    cell.className = 'state-table-expand-button-cell';
    var button = document.createElement('button');
    button.addEventListener('click', function(event) {
      toggleExpandRow(/** @type {!HTMLElement} */(event.target), guid);
    });
    button.className = 'state-table-expand-button';
    button.textContent = '+';
    cell.appendChild(button);
    return cell;
  };

  /**
   * Creates a cell with an icon representing the network state.
   *
   * @param {!chrome.networkingPrivate.NetworkStateProperties} networkState The
   *     network state properties.
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     icon.
   */
  var createStateTableIcon = function(networkState) {
    var cell = createTableCellElement();
    cell.className = 'state-table-icon-cell';
    var icon = /** @type {!CrNetworkIconElement} */(
        document.createElement('cr-network-icon'));
    icon.isListItem = true;
    icon.networkState = networkState;
    cell.appendChild(icon);
    return cell;
  };

  /**
   * Creates a cell in the network state table.
   *
   * @param {*} value Content in the cell.
   * @return {!HTMLTableCellElement} The created td element that displays the
   *     given value.
   */
  var createStateTableCell = function(value) {
    var cell = createTableCellElement();
    cell.textContent = value || '';
    return cell;
  };

  /**
   * Creates a row in the network state table.
   *
   * @param {Array} stateFields The state fields to use for the row.
   * @param {!chrome.networkingPrivate.NetworkStateProperties} networkState The
   *     network state properties.
   * @return {!HTMLTableRowElement} The created tr element that contains the
   *     network state information.
   */
  var createStateTableRow = function(stateFields, networkState) {
    var row = createTableRowElement();
    row.className = 'state-table-row';
    var guid = networkState.GUID;
    row.appendChild(createStateTableExpandButton(guid));
    row.appendChild(createStateTableIcon(networkState));
    for (var i = 0; i < stateFields.length; ++i) {
      var field = stateFields[i];
      var value;
      if (typeof field == 'string') {
        value = getOncProperty(networkState, field);
      } else {
        for (var j = 0; j < field.length; ++j) {
          value = getOncProperty(networkState, field[j]);
          if (value != undefined)
            break;
        }
      }
      if (field == 'GUID')
        value = value.slice(0, 8);
      row.appendChild(createStateTableCell(value));
    }
    return row;
  };

  /**
   * Creates a table for networks or favorites.
   *
   * @param {string} tablename The name of the table to be created.
   * @param {!Array<string>} stateFields The list of fields for the table.
   * @param {!Array<!chrome.networkingPrivate.NetworkStateProperties>} states
   *     An array of network or favorite states.
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
   * @param {string} guid A GUID which may start with a digit.
   * @return {string} A valid HTMLElement id.
   */
  var idFromGuid = function(guid) {
    return '_' + guid.replace(/[{}]/g, '');
  };

  /**
   * This callback function is triggered when visible networks are received.
   *
   * @param {!Array<!chrome.networkingPrivate.NetworkStateProperties>} states
   *     A list of network state information for each visible network.
   */
  var onVisibleNetworksReceived = function(states) {
    createStateTable('network-state-table', NETWORK_STATE_FIELDS, states);
  };

  /**
   * This callback function is triggered when favorite networks are received.
   *
   * @param {!Array<!chrome.networkingPrivate.NetworkStateProperties>} states
   *     A list of network state information for each favorite network.
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
    var row = /** @type {!HTMLTableRowElement} */(cell.parentNode);
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
   * @param {!HTMLTableRowElement} baseRow The unexpanded row associated with
   *     the new row.
   * @return {!HTMLTableRowElement} The created tr element for the expanded row.
   */
  var createExpandedRow = function(guid, baseRow) {
    var expandedRow = createTableRowElement();
    expandedRow.className = 'state-table-row';
    var emptyCell = createTableCellElement();
    emptyCell.style.border = 'none';
    expandedRow.appendChild(emptyCell);
    var detailCell = createTableCellElement();
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
    } else if (selectedId == 'state') {
      chrome.networkingPrivate.getState(guid, function(properties) {
        showDetail(properties, chrome.runtime.lastError); });
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
        {'networkType': chrome.networkingPrivate.NetworkType.ALL,
         'visible': true},
        onVisibleNetworksReceived);
    chrome.networkingPrivate.getNetworks(
        {'networkType': chrome.networkingPrivate.NetworkType.ALL,
         'configured': true},
        onFavoriteNetworksReceived);
  };

  /**
   * Requests the global policy dictionary and updates the page.
   */
  var requestGlobalPolicy = function() {
    chrome.networkingPrivate.getGlobalPolicy(function(policy) {
      document.querySelector('#global-policy').textContent =
          JSON.stringify(policy, null, '\t');
    });
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
   * Gets network information from WebUI and sets custom items.
   */
  document.addEventListener('DOMContentLoaded', function() {
    let select = document.querySelector('cr-network-select');
    select.customItems = [
      {customItemName: 'Add WiFi', polymerIcon: 'cr:add', customData: 'WiFi'},
      {customItemName: 'Add VPN', polymerIcon: 'cr:add', customData: 'VPN'}
    ];
    $('refresh').onclick = requestNetworks;
    setRefresh();
    requestNetworks();
    requestGlobalPolicy();
  });

  document.addEventListener('custom-item-selected', function(event) {
    chrome.send('addNetwork', [event.detail.customData]);
  });

  return {
    getShillPropertiesResult: getShillPropertiesResult
  };
})();
