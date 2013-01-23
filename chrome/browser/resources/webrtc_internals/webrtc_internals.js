// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var peerConnectionsListElem = null;

function initialize() {
  peerConnectionsListElem = $('peer-connections-list');
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

//
// Browser message handlers
//

function removePeerConnection(data) {
  var element = $(getPeerConnectionId(data));
  if (element)
    peerConnectionsListElem.removeChild(element);
}

function addPeerConnection(data) {
  var peerConnectionElement = ensurePeerConnectionElement(
      getPeerConnectionId(data));
  peerConnectionElement.innerHTML = 'PeerConnection ' +
                                     peerConnectionElement.id + '<br>' +
                                     data.url + ', ' +
                                     data.servers + ', ' +
                                     data.constraints + '<br>';
}

document.addEventListener('DOMContentLoaded', initialize);
