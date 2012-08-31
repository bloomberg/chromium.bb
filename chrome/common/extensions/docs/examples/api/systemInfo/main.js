// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var systemInfo = chrome.experimental.systemInfo;

var indicator = {}
var isStarted = false;

function onStorageChanged(info) {
  var elem = document.getElementById(info.id);
  if (indicator[info.id]++ % 2)
    elem.bgColor = "green";
  else
    elem.bgColor = "white";
  elem.innerHTML = info.availableCapacity;
}

function startMonitor() {
  if (isStarted) return;
  systemInfo.storage.onAvailableCapacityChanged.addListener(onStorageChanged);
  isStarted = true;
}

function stopMonitor() {
  if (!isStarted) return;
  systemInfo.storage.onAvailableCapacityChanged.removeListener(
      onStorageChanged);
  isStarted = false;
}

function init() {
  document.getElementById("start-btn").onclick = startMonitor;
  document.getElementById("stop-btn").onclick = stopMonitor;

  chrome.experimental.systemInfo.storage.get(function(units) {
    var table = "<table width=65% border=\"1\">\n" +
      "<tr><td><b>ID</b></td>" +
      "<td>Type</td>" +
      "<td>Capacity (bytes)</td>" +
      "<td>Available (bytes)</td>" +
      "</tr>\n";
    for (var i = 0; i < units.length; i++) {
      indicator[units[i].id] = 0;
      table += showStorageInfo(units[i]);
    }
    table += "</table>\n";
    var div = document.getElementById("storage-list");
    div.innerHTML = table;
  });
}

function showStorageInfo(unit) {
  table = "<tr><td>" + unit.id + "</td>" +
    "<td>" + unit.type + "</td>" +
    "<td>" + unit.capacity + "</td>" +
    "<td id=" + "\"" + unit.id + "\">" + unit.availableCapacity + "</td>" +
    "</tr>\n";
  return table;
}

document.addEventListener('DOMContentLoaded', init);
