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

function populateWorkerList() {
  var data = requestData();

  var worker_properties = ["workerRouteId", "url", "name", "pid"];

  var list = document.getElementById("workers-table");
  for (var i = 0; i < data.length; i++) {
    var workerData = data[i];
    var row = document.createElement("tr");
    for (var j = 0; j < worker_properties.length; j++)
      addColumn(row, workerData[worker_properties[j]]);

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

    list.appendChild(row);
  }
}

document.addEventListener('DOMContentLoaded', populateWorkerList);
