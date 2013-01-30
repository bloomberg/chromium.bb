// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var peerConnectionsListElem = null;

function initialize() {
  peerConnectionsListElem = $('peer-connections-list');
  chrome.send('getAllUpdates');
}

function getPeerConnectionId(data) {
  return data.pid + ':' + data.lid;
}

// Makes sure a LI element representing a PeerConnection is created
// and appended to peerConnectionListElem.
function ensurePeerConnectionElement(id) {
  var element = $(id);
  if (!element) {
    element = document.createElement('li');
    peerConnectionsListElem.appendChild(element);
    element.id = id;
  }
  return element;
}

// Makes sure the table representing the PeerConnection event log is created
// and appended to peerConnectionElement.
function ensurePeerConnectionLog(peerConnectionElement) {
  var logId = peerConnectionElement.id + ':log';
  var logElement = $(logId);
  if (!logElement) {
    logElement = document.createElement('table');
    logElement.id = logId;
    logElement.class = 'log-table';
    logElement.border = 1;
    peerConnectionElement.appendChild(logElement);
    logElement.innerHTML = '<tr><th>Time</th>' +
                               '<th class="log-header-event">Event</th></tr>';
  }
  return logElement;
}

// Add the update to the log table as a new row. The type of the update is set
// as the text of the cell; clicking the cell will reveal or hide the details
// as the content of a TextArea element.
function addToPeerConnectionLog(logElement, update) {
  var row = document.createElement('tr');
  logElement.appendChild(row);

  var expandable = (update.value.length > 0);
  row.innerHTML = '<td>' + (new Date()).toLocaleString() + '</td>' +
                  '<td><span>' + update.type + '</span>' +
                  (expandable ? '<button>Expand/Collapse</button>' : '') +
                  '</td>';
  if (!expandable)
    return;

  var valueContainer = document.createElement('textarea');
  row.cells[1].appendChild(valueContainer);
  valueContainer.value = update.value;
  valueContainer.style.height = valueContainer.scrollHeight + 'px';
  valueContainer.style.display = 'none';

  // Hide or reveal the update details on clicking the button.
  row.cells[1].childNodes[1].addEventListener('click', function(event) {
      var element = event.target.nextSibling;
      if (element.style.display == 'none') {
        element.style.display = 'block';
      }
      else {
        element.style.display = 'none';
      }
  });
}

//
// Browser message handlers.
//

// data = {pid:|integer|, lid:|integer|}.
function removePeerConnection(data) {
  var element = $(getPeerConnectionId(data));
  if (element)
    peerConnectionsListElem.removeChild(element);
}

// data = {pid:|integer|, lid:|integer|,
//         url:|string|, servers:|string|, constraints:|string|}.
function addPeerConnection(data) {
  var peerConnectionElement = ensurePeerConnectionElement(
      getPeerConnectionId(data));
  peerConnectionElement.innerHTML =
      '<h3>PeerConnection ' + peerConnectionElement.id + '</h3>' +
      '<div>' + data.url + ' ' + data.servers + ' ' + data.constraints +
      '</div>';
  return peerConnectionElement;
}

// data = {pid:|integer|, lid:|integer|, type:|string|, value:|string|}.
function updatePeerConnection(data) {
  var peerConnectionElement = ensurePeerConnectionElement(
      getPeerConnectionId(data));
  var logElement = ensurePeerConnectionLog(peerConnectionElement);
  addToPeerConnectionLog(logElement, data);
}

// data is an array and each entry is in the same format as the input of
// updatePeerConnection.
function updateAllPeerConnections(data) {
  for (var i = 0; i < data.length; ++i) {
    var peerConnection = addPeerConnection(data[i]);
    var logElement = ensurePeerConnectionLog(peerConnection);
    logElement.value = '';

    var log = data[i].log;
    for (var j = 0; j < log.length; ++j) {
      addToPeerConnectionLog(logElement, log[j]);
    }
  }
}

document.addEventListener('DOMContentLoaded', initialize);
