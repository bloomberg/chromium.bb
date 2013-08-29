// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for local_discovery.html, served from chrome://devices/
 * This is used to show discoverable devices near the user.
 *
 * The simple object defined in this javascript file listens for
 * callbacks from the C++ code saying that a new device is available.
 */

cr.define('local_discovery', function() {
  'use strict';

  /**
   * Add a text TD to a TR.
   * @param {HTMLTableRowElement} row Row in table to be filled.
   * @param {string} text Text of TD.
   */
  function textTD(row, text) {
    var td = document.createElement('td');
    td.textContent = text;
    row.appendChild(td);
  }

  /**
   * Add a button TD to a TR.
   * @param {HTMLTableRowElement} row Row in table to be filled.
   * @param {string} text Text of button.
   * @param {function()} action Action to be taken when button is pressed.
   */
  function buttonTD(row, text, action) {
    var td = document.createElement('td');
    var button = document.createElement('button');
    button.textContent = text;
    button.addEventListener('click', action);
    td.appendChild(button);
    row.appendChild(td);
  }

  /**
   * Fill a table row from the provided information.
   * @param {HTMLTableRowElement} row Row in table to be filled.
   * @param {string} name Name of the device.
   * @param {Object} info Information about the device.
   */
  function fillRow(row, name, info) {
    textTD(row, name);
    textTD(row, info.domain);
    textTD(row, info.port);
    textTD(row, info.ip);
    textTD(row, info.lastSeen);

    if (!info.registered) {
      buttonTD(row, loadTimeData.getString('serviceRegister'),
               sendRegisterDevice.bind(null, name));
    } else {
      textTD(row, loadTimeData.getString('registered'));
    }

    buttonTD(row, loadTimeData.getString('serviceInfo'),
             sendInfoRequest.bind(null, name));
  }

  /**
   * Appends a row to the output table listing the new device.
   * @param {string} name Name of the device.
   * @param {string} info Additional info of the device, if empty device need to
   *                      be deleted.
   */
  function onServiceUpdate(name, info) {
    name = name.replace(/[\r\n]/g, '');
    var table = $('devices-table');

    for (var i = 0, row; row = table.rows[i]; i++) {
      if (row.cells[0].textContent == name) {
        if (!info) {
          // Delete service from the row.
          table.removeChild(row);
        } else {
          // Replace existing service.
          while (row.firstChild) {
            row.removeChild(row.firstChild);
          }

          fillRow(row, name, info);
          return;
        }
      }
    }

    if (!info) {
      // Service could not be found in the table.
      return;
    }

    // Row does not exist. Create it.
    var tr = document.createElement('tr');

    fillRow(tr, name, info);
    table.appendChild(tr);
  }

  /**
   * Adds a row to the logging console.
   * @param {string} msg The message to log.
   */
  function logToInfoConsole(msg) {
    var div = document.createElement('div');
    div.textContent = msg;
    $('info-console').appendChild(div);
  }

  /**
   * Register a device.
   * @param {string} device The device to register.
   */
  function sendRegisterDevice(device) {
    chrome.send('registerDevice', [device]);
    logToInfoConsole(loadTimeData.getStringF('registeringService', device));
  }

  /**
   * Announce that a registration failed.
   * @param {string} reason The error message.
   */
  function registrationFailed(reason) {
    logToInfoConsole(loadTimeData.getStringF('registrationFailed', reason));
  }

  /**
   * Request the contents of a device's /info page.
   * @param {string} device The device to query.
   */
  function sendInfoRequest(device) {
    chrome.send('info', [device]);
    logToInfoConsole(loadTimeData.getStringF('infoStarted', device));
  }

  /**
   * Announce that a registration succeeeded.
   * @param {string} id The id of the newly registered device.
   */
  function registrationSuccess(id) {
    logToInfoConsole(loadTimeData.getStringF('registrationSucceeded', id));
  }

  /**
   * Render an info item onto the info pane.
   * @param {string} name Name of the item.
   * @param {?} value Value of the item.
   * @param {function(?):string} render_type Render function for value
   *     datatype.
   * @return {HTMLElement} Rendered info item.
   */
  function renderInfoItem(name, value, render_type) {
    var container = document.createElement('div');
    container.classList.add('info-item');
    var nameElem = document.createElement('span');
    nameElem.classList.add('info-item-name');
    nameElem.textContent = name;
    container.appendChild(nameElem);
    var valueElem = document.createElement('span');
    valueElem.textContent = render_type(value);
    container.appendChild(valueElem);
    return container;
  }

  /**
   * Render and append an info item to the info pane, if it exists.
   * @param {Object} info Info response.
   * @param {string} name Name of property.
   * @param {function(?):string} render_type Render function for value.
   */
  function infoItem(info, name, render_type) {
    if (name in info) {
      $('info-pane').appendChild(renderInfoItem(name, info[name], render_type));
    }
  }

  /**
   * Render a string to an info-pane-displayable string.
   * @param {?} value Value; not guaranteed to be a string.
   * @return {string} Rendered value.
   */
  function renderTypeString(value) {
    if (typeof value != 'string') {
      return 'INVALID';
    }
    return value;
  }

  /**
   * Render a integer to an info-pane-displayable string.
   * @param {?} value Value; not guaranteed to be an integer.
   * @return {string} Rendered value.
   */
  function renderTypeInt(value) {
    if (typeof value != 'number') {
      return 'INVALID';
    }

    return value.toString();
  }

  /**
   * Render an array to an info-pane-displayable string.
   * @param {?} value Value; not guaranteed to be an array.
   * @return {string} Rendered value.
   */
  function renderTypeStringList(value) {
    if (!Array.isArray(value)) {
      return 'INVALID';
    }

    var returnValue = '';
    var valueLength = value.length;
    for (var i = 0; i < valueLength - 1; i++) {
      returnValue += value[i];
      returnValue += ', ';
    }

    if (value.length != 0) {
      returnValue += value[value.length - 1];
    }

    return returnValue;
  }

  /**
   * Render info response from JSON.
   * @param {Object} info Info response.
   */
  function renderInfo(info) {
    // Clear info
    while ($('info-pane').firstChild) {
      $('info-pane').removeChild($('info-pane').firstChild);
    }

    infoItem(info, 'x-privet-token', renderTypeString);
    infoItem(info, 'id', renderTypeString);
    infoItem(info, 'name', renderTypeString);
    infoItem(info, 'description', renderTypeString);
    infoItem(info, 'type', renderTypeStringList);
    infoItem(info, 'api', renderTypeStringList);
    infoItem(info, 'connection_state', renderTypeString);
    infoItem(info, 'device_state', renderTypeString);
    infoItem(info, 'manufacturer', renderTypeString);
    infoItem(info, 'url', renderTypeString);
    infoItem(info, 'model', renderTypeString);
    infoItem(info, 'serial_number', renderTypeString);
    infoItem(info, 'firmware', renderTypeString);
    infoItem(info, 'uptime', renderTypeInt);
    infoItem(info, 'setup_url', renderTypeString);
    infoItem(info, 'support_url', renderTypeString);
    infoItem(info, 'update_url', renderTypeString);
  }

  /**
   * Announce that an info request failed.
   * @param {string} reason The error message.
   */
  function infoFailed(reason) {
    logToInfoConsole(loadTimeData.getStringF('infoFailed', reason));
  }

  /*
   * Update visibility status for page.
   */
  function updateVisibility() {
    chrome.send('isVisible', [!document.webkitHidden]);
  }

  /*
   * Request a user account from a list.
   * @param {Array} users Array of (index, username) tuples. Username may be
   *    displayed to the user; index must be passed opaquely to the UI handler.
   */
  function requestUser(users) {
    while ($('user-list-container').firstChild) {
      $('user-list-container').removeChild($('user-list-container').firstChild);
    }

    var usersLength = users.length;
    for (var i = 0; i < usersLength; i++) {
      var userIndex = users[i][0];
      var userName = users[i][1];

      var button = document.createElement('button');
      button.textContent = userName;
      button.addEventListener('click',
                              selectUser.bind(null, userIndex, userName));
      $('user-list-container').appendChild(button);
    }
  }

  function selectUser(userIndex, userName) {
    while ($('user-list-container').firstChild) {
      $('user-list-container').removeChild($('user-list-container').firstChild);
    }

    chrome.send('chooseUser', [userIndex, userName]);
  }

  document.addEventListener('DOMContentLoaded', function() {
    updateVisibility();
    document.addEventListener('webkitvisibilitychange', updateVisibility,
                              false);
    chrome.send('start');
  });

  return {
    registrationSuccess: registrationSuccess,
    registrationFailed: registrationFailed,
    onServiceUpdate: onServiceUpdate,
    infoFailed: infoFailed,
    renderInfo: renderInfo,
    requestUser: requestUser
  };
});
