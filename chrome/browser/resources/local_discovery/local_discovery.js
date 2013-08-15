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
   * Appends a row to the output table listing the new device.
   * @param {string} name Name of the device.
   * @param {string} info Additional info of the device, if empty device need to
   *    be deleted.
   */
  function onServiceUpdate(name, info) {
    name = name.replace(/[\r\n]/g, '');
    var table = $('devices-table');

    var params = [];
    if (info) {
      params[0] = info.domain;
      params[1] = info.port;
      params[2] = info.ip;
      params[3] = info.lastSeen;
    }

    for (var i = 0, row; row = table.rows[i]; i++) {
      if (row.cells[0].textContent == name) {
        if (!info) {
          // Delete service from the row.
          table.removeChild(row);
        } else {
          // Replace existing service.
          for (var j = 0; j < params.length; j++) {
            row.cells[j + 1].textContent = params[j];
          }
        }
        return;
      }
    }

    if (!info) {
      // Service could not be found in the table.
      return;
    }

    var tr = document.createElement('tr');
    var td = document.createElement('td');
    td.textContent = name;
    tr.appendChild(td);

    for (var j = 0; j < params.length; j++) {
      td = document.createElement('td');
      td.textContent = params[j];
      tr.appendChild(td);
    }

    td = document.createElement('td');
    if (!info.registered) {
      var button = document.createElement('button');
      button.textContent = loadTimeData.getString('serviceRegister');
      button.addEventListener('click', sendRegisterDevice.bind(null, name));
      td.appendChild(button);
    } else {
      td.textContent = loadTimeData.getString('registered');
    }
    tr.appendChild(td);

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
   * Announce that a registration succeeeded.
   * @param {string} id The id of the newly registered device.
   */
  function registrationSuccess(id) {
    logToInfoConsole(loadTimeData.getStringF('registrationSucceeded', id));
  }

  document.addEventListener('DOMContentLoaded', function() {
    chrome.send('start');
  });

  return {
    registrationSuccess: registrationSuccess,
    registrationFailed: registrationFailed,
    onServiceUpdate: onServiceUpdate
  };
});
