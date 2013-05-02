// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var peerConnectionsListElem = null;
var ssrcInfoManager = null;
var peerConnectionUpdateTable = null;
var statsTable = null;
var dumpCreator = null;

<include src="ssrc_info_manager.js"/>
<include src="stats_graph_helper.js"/>
<include src="stats_table.js"/>
<include src="peer_connection_update_table.js"/>
<include src="dump_creator.js"/>

function initialize() {
  peerConnectionsListElem = $('peer-connections-list');
  dumpCreator = new DumpCreator(peerConnectionsListElem);
  ssrcInfoManager = new SsrcInfoManager();
  peerConnectionUpdateTable = new PeerConnectionUpdateTable();
  statsTable = new StatsTable(ssrcInfoManager);

  chrome.send('getAllUpdates');

  // Requests stats from all peer connections every second.
  window.setInterval(function() {
    if (peerConnectionsListElem.getElementsByTagName('li').length > 0)
      chrome.send('getAllStats');
  }, 1000);
}
document.addEventListener('DOMContentLoaded', initialize);


/**
 * A helper function for getting a peer connection element id.
 *
 * @param {!Object.<string, number>} data The object containing the pid and lid
 *     of the peer connection.
 * @return {string} The peer connection element id.
 */
function getPeerConnectionId(data) {
  return data.pid + '-' + data.lid;
}


/**
 * Extracts ssrc info from a setLocal/setRemoteDescription update.
 *
 * @param {!PeerConnectionUpdateEntry} data The peer connection update data.
 */
function extractSsrcInfo(data) {
  if (data.type == 'setLocalDescription' ||
      data.type == 'setRemoteDescription') {
    ssrcInfoManager.addSsrcStreamInfo(data.value);
  }
}


/**
 * Browser message handlers.
 */


/**
 * Removes all information about a peer connection.
 *
 * @param {!Object.<string, number>} data The object containing the pid and lid
 *     of a peer connection.
 */
function removePeerConnection(data) {
  var element = $(getPeerConnectionId(data));
  if (element)
    peerConnectionsListElem.removeChild(element);
}


/**
 * Adds a peer connection.
 *
 * @param {!Object} data The object containing the pid, lid, url, servers, and
 *     constraints of a peer connection.
 */
function addPeerConnection(data) {
  var peerConnectionElement = $(getPeerConnectionId(data));
  if (!peerConnectionElement) {
    peerConnectionElement = document.createElement('li');
    peerConnectionsListElem.appendChild(peerConnectionElement);
    peerConnectionElement.id = getPeerConnectionId(data);
  }
  peerConnectionElement.innerHTML =
      '<h3>PeerConnection ' + peerConnectionElement.id + '</h3>' +
      '<div>' + data.url + ' ' + data.servers + ' ' + data.constraints +
      '</div>';

  // Clicking the heading can expand or collapse the peer connection item.
  peerConnectionElement.firstChild.title = 'Click to collapse or expand';
  peerConnectionElement.firstChild.addEventListener('click', function(e) {
    if (e.target.parentElement.className == '')
      e.target.parentElement.className = 'peer-connection-hidden';
    else
      e.target.parentElement.className = '';
  });
  return peerConnectionElement;
}


/**
 * Adds a peer connection update.
 *
 * @param {!PeerConnectionUpdateEntry} data The peer connection update data.
 */
function updatePeerConnection(data) {
  var peerConnectionElement = $(getPeerConnectionId(data));
  peerConnectionUpdateTable.addPeerConnectionUpdate(
      peerConnectionElement, data);
  extractSsrcInfo(data);
}


/**
 * Adds the information of all peer connections created so far.
 *
 * @param {Array.<!Object>} data An array of the information of all peer
 *     connections. Each array item contains pid, lid, url, servers,
 *     constraints, and an array of updates as the log.
 */
function updateAllPeerConnections(data) {
  for (var i = 0; i < data.length; ++i) {
    var peerConnection = addPeerConnection(data[i]);

    var log = data[i].log;
    for (var j = 0; j < log.length; ++j) {
      peerConnectionUpdateTable.addPeerConnectionUpdate(
          peerConnection, log[j]);
      extractSsrcInfo(log[j]);
    }
  }
}


/**
 * Handles the report of stats.
 *
 * @param {!Object} data The object containing pid, lid, and reports, where
 *     reports is an array of stats reports. Each report contains id, type,
 *     and stats, where stats is the object containing timestamp and values,
 *     which is an array of strings, whose even index entry is the name of the
 *     stat, and the odd index entry is the value.
 */
function addStats(data) {
  var peerConnectionElement = $(getPeerConnectionId(data));
  if (!peerConnectionElement)
    return;

  for (var i = 0; i < data.reports.length; ++i) {
    var report = data.reports[i];
    statsTable.addStatsReport(peerConnectionElement, report);
    drawSingleReport(peerConnectionElement,
                     report.type, report.id, report.stats);
  }
}


/**
 * Delegates to dumpCreator to update the recording status.
 * @param {!Object.<string>} update Key-value pairs describing the status of the
 *     RTP recording.
 */
function updateDumpStatus(update) {
  dumpCreator.onUpdate(update);
}
