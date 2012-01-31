// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shows an updating list of process statistics.
function init() {
  chrome.experimental.processes.onUpdated.addListener(function(processes) {
    var table = "<table>\n" +
      "<tr><td><b>Process</b></td>" +
      "<td>Type</td>" +
      "<td>CPU</td>" +
      "<td>Network</td>" +
      "<td>Shared Memory</td>" +
      "<td>Private Memory</td>" +
      "</tr>\n";
    for (pid in processes) {
      table = displayProcessInfo(processes[pid], table);
    }
    table += "</table>\n";
    var div = document.getElementById("process-list");
    div.innerHTML = table;
  });
}

function displayProcessInfo(process, table) {
  // Format network string like task manager
  var network = process.network;
  if (network > 1024) {
    network = (network / 1024).toFixed(1) + " kB/s";
  } else if (network > 0) {
    network += " B/s";
  } else if (network == -1) {
    network = "N/A";
  }

  table +=
    "<tr><td>" + process.id + "</td>" +
    "<td>" + process.type + "</td>" +
    "<td>" + process.cpu + "</td>" +
    "<td>" + network + "</td>" +
    "<td>" + (process.sharedMemory / 1024) + "K</td>" +
    "<td>" + (process.privateMemory / 1024) + "K</td>" +
    "</tr>\n";
  return table;
}

document.addEventListener('DOMContentLoaded', init);