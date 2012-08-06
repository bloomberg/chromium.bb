// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Updates the Connectivity Status section.
 * @param {String} connStatus Dictionary containing connectivity status.
 */
function updateConnectivityStatus(connStatus) {
  var deviceTypes = ['wlan0', 'wwan0', 'eth0', 'eth1'];
  var deviceNames = ['Wi-Fi', '3G', 'Ethernet0', 'Ethernet1'];
  for (var i = 0; i < deviceTypes.length; i++) {
    var deviceName = deviceNames[i];
    var nameElement = document.createElement('h2');
    nameElement.appendChild(document.createTextNode(deviceName));
    $('connectivity-status').appendChild(nameElement);

    var deviceType = deviceTypes[i];
    var deviceStatus = connStatus[deviceType];
    var statusMessage;
    if (!deviceStatus) {
      statusMessage = 'Device not found.';
    } else if (!deviceStatus.flags ||
               deviceStatus.flags.indexOf('up') == -1) {
      statusMessage = 'Device disabled.';
    } else if (!deviceStatus.ipv4) {
      statusMessage = 'IPv4 address unavailable.';
    } else {
      statusMessage = 'IPv4 address: ' + deviceStatus.ipv4.addrs;
    }
    var statusElement = document.createElement('p');
    statusElement.appendChild(document.createTextNode(statusMessage));
    $('connectivity-status').appendChild(statusElement);
  }
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.send('pageLoaded');
});
