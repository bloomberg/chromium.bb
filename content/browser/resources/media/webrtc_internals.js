// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var peerConnectionsListElem = null;
var ssrcInfoManager = null;
var peerConnectionUpdateTable = null;
var statsTable = null;
var dumpCreator = null;
/** A map from peer connection id to the PeerConnectionRecord. */
var peerConnectionDataStore = {};

/** A simple class to store the updates and stats data for a peer connection. */
var PeerConnectionRecord = (function() {
  /** @constructor */
  function PeerConnectionRecord() {
    /** @private */
    this.record_ = {
      constraints: {},
      servers: [],
      stats: {},
      updateLog: [],
      url: '',
    };
  };

  PeerConnectionRecord.prototype = {
    /** @override */
    toJSON: function() {
      return this.record_;
    },

    /**
     * Adds the initilization info of the peer connection.
     * @param {string} url The URL of the web page owning the peer connection.
     * @param {Array} servers STUN servers used by the peer connection.
     * @param {!Object} constraints Media constraints.
     */
    initialize: function(url, servers, constraints) {
      this.record_.url = url;
      this.record_.servers = servers;
      this.record_.constraints = constraints;
    },

    /**
     * @param {string} dataSeriesId The TimelineDataSeries identifier.
     * @return {!TimelineDataSeries}
     */
    getDataSeries: function(dataSeriesId) {
      return this.record_.stats[dataSeriesId];
    },

    /**
     * @param {string} dataSeriesId The TimelineDataSeries identifier.
     * @param {!TimelineDataSeries} dataSeries The TimelineDataSeries to set to.
     */
    setDataSeries: function(dataSeriesId, dataSeries) {
      this.record_.stats[dataSeriesId] = dataSeries;
    },

    /**
     * @param {string} type The type of the update.
     * @param {string} value The value of the update.
     */
    addUpdate: function(type, value) {
      this.record_.updateLog.push({
        time: (new Date()).toLocaleString(),
        type: type,
        value: value,
      });
    },
  };

  return PeerConnectionRecord;
})();

// The maximum number of data points bufferred for each stats. Old data points
// will be shifted out when the buffer is full.
var MAX_STATS_DATA_POINT_BUFFER_SIZE = 1000;

<include src="data_series.js"/>
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
 * Helper for adding a peer connection update.
 *
 * @param {Element} peerConnectionElement
 * @param {!PeerConnectionUpdateEntry} update The peer connection update data.
 */
function addPeerConnectionUpdate(peerConnectionElement, update) {
  peerConnectionUpdateTable.addPeerConnectionUpdate(peerConnectionElement,
                                                    update);
  extractSsrcInfo(update);
  peerConnectionDataStore[peerConnectionElement.id].addUpdate(
      update.type, update.value);
}


/** Browser message handlers. */


/**
 * Removes all information about a peer connection.
 *
 * @param {!Object.<string, number>} data The object containing the pid and lid
 *     of a peer connection.
 */
function removePeerConnection(data) {
  var element = $(getPeerConnectionId(data));
  if (element) {
    delete peerConnectionDataStore[element.id];
    peerConnectionsListElem.removeChild(element);
  }
}


/**
 * Adds a peer connection.
 *
 * @param {!Object} data The object containing the pid, lid, url, servers, and
 *     constraints of a peer connection.
 */
function addPeerConnection(data) {
  var id = getPeerConnectionId(data);

  if (!peerConnectionDataStore[id]) {
    peerConnectionDataStore[id] = new PeerConnectionRecord();
  }
  peerConnectionDataStore[id].initialize(
      data.url, data.servers, data.constraints);

  var peerConnectionElement = $(id);
  if (!peerConnectionElement) {
    peerConnectionElement = document.createElement('li');
    peerConnectionsListElem.appendChild(peerConnectionElement);
    peerConnectionElement.id = id;
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
  addPeerConnectionUpdate(peerConnectionElement, data);
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
      addPeerConnectionUpdate(peerConnection, log[j]);
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
    drawSingleReport(peerConnectionElement, report);
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
