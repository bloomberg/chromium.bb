// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function requestData() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'workers_data.json', false);
  xhr.send(null);
  if (xhr.status === 200)
     return JSON.parse(xhr.responseText);
  return [];
}

function addColumn(row, value) {
  var column = document.createElement("td");
  column.textContent = value;
  row.appendChild(column);
}

function openDevTools(workerProcessHostId, workerRouteId) {
  chrome.send("openDevTools",
              [String(workerProcessHostId), String(workerRouteId)]);
}

function reloadWorker(workerProcessHostId, workerRouteId) {
  chrome.send("terminateWorker",
              [String(workerProcessHostId), String(workerRouteId)]);
}

function populateWorkerList() {
  var data = requestData();
  var list = document.getElementById("workers-table");
  for (var i = 0; i < data.length; i++)
    addWorkerInfoToList(data[i], list);
}

function addWorkerInfoToList(workerData, list) {
  var row = document.createElement("tr");
  var workerProperties = ["workerRouteId", "url", "name", "pid"];
  for (var j = 0; j < workerProperties.length; j++)
    addColumn(row, workerData[workerProperties[j]]);

  var column = document.createElement("td");
  var link = document.createElement("a");
  link.setAttribute("href", "#");
  link.textContent = "inspect";
  link.addEventListener(
      "click",
      openDevTools.bind(this,
                        workerData.workerProcessHostId,
                        workerData.workerRouteId),
      true);
  column.appendChild(link);
  row.appendChild(column);

  var link = document.createElement("a");
  link.setAttribute("href", "#");
  link.textContent = "terminate";
  link.addEventListener(
      "click",
      reloadWorker.bind(this,
                        workerData.workerProcessHostId,
                        workerData.workerRouteId),
      true);
  column.appendChild(link);
  row.appendChild(column);

  row.workerProcessHostId = workerData.workerProcessHostId;
  row.workerRouteId = workerData.workerRouteId;

  list.appendChild(row);
}

function workerCreated(workerData) {
  var list = document.getElementById("workers-table");
  addWorkerInfoToList(workerData, list);
}

function workerDestroyed(workerData) {
  var list = document.getElementById("workers-table");
  for (var row = list.firstChild; row; row = row.nextSibling) {
    if (row.workerProcessHostId === workerData.workerProcessHostId &&
        row.workerRouteId === workerData.workerRouteId) {
      list.removeChild(row);
      return;
    }
  }
}

document.addEventListener('DOMContentLoaded', populateWorkerList);
