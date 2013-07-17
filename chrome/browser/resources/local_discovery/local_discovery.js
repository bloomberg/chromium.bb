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

/**
 * Appends a row to the output table listing the new device.
 * @param {string} name of the device service.
 * @param {string} info - additional info of the device,
 *                        if empty device need to be deleted
 */
function onServiceUpdate(name, info) {
  var table = $('devices-table');

  var params = [];
  if (info) {
    params[0] = info.domain;
    params[1] = info.port;
    params[2] = info.ip;
    params[3] = info.metadata;
    params[4] = info.lastSeen;
    params[5] = info.registered;
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

  table.appendChild(tr);
}
