// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the state of all socket pools.
 *
 *   - Shows a summary of the state of each socket pool at the top.
 *   - For each pool with allocated sockets or connect jobs, shows all its
 *     groups with any allocated sockets.
 *
 *  @constructor
 */
function SocketsView(mainBoxId, socketPoolsTbodyId, socketPoolGroupsDivId) {
  DivView.call(this, mainBoxId);

  g_browser.addSocketPoolInfoObserver(this);
  this.socketPoolsTbody_ = document.getElementById(socketPoolsTbodyId);
  this.socketPoolGroupsDiv_ = document.getElementById(socketPoolGroupsDivId);
}

inherits(SocketsView, DivView);

SocketsView.prototype.onSocketPoolInfoChanged = function(socketPoolInfo) {
  this.socketPoolsTbody_.innerHTML = '';
  this.socketPoolGroupsDiv_.innerHTML = '';

  if (!socketPoolInfo)
    return;

  for (var i = 0; i < socketPoolInfo.length; ++i) {
    this.addSocketPool_(socketPoolInfo[i]);
  }
};

// Adds the socket pool to the bottom of the main table and, if there are
// any groups, creates a table at the bottom listing those as well.
SocketsView.prototype.addSocketPool_ = function(socketPool) {
  var socketPoolRow = addNode(this.socketPoolsTbody_, 'tr');

  var socketPoolName = socketPool.name;
  if (socketPoolName != socketPool.type)
    socketPoolName += ' (' + socketPool.type + ')';

  addNodeWithText(socketPoolRow, 'td', socketPoolName);
  addNodeWithText(socketPoolRow, 'td', socketPool.connecting_socket_count);
  addNodeWithText(socketPoolRow, 'td', socketPool.handed_out_socket_count);
  addNodeWithText(socketPoolRow, 'td', socketPool.idle_socket_count);
  addNodeWithText(socketPoolRow, 'td', socketPool.max_socket_count);
  addNodeWithText(socketPoolRow, 'td', socketPool.max_sockets_per_group);
  addNodeWithText(socketPoolRow, 'td', socketPool.pool_generation_number);

  if (socketPool.groups != undefined)
    this.addSocketPoolGroupTable_(socketPoolName, socketPool);
};

// Creates a table at the bottom listing all a table's groups.
SocketsView.prototype.addSocketPoolGroupTable_ = function(socketPoolName,
                                                          socketPool) {
  addNodeWithText(this.socketPoolGroupsDiv_, 'h4', socketPoolName);
  var table = addNode(this.socketPoolGroupsDiv_, 'table');
  table.setAttribute('class', 'styledTable');

  var thead = addNode(table, 'thead');
  var headerRow = addNode(thead, 'tr');
  addNodeWithText(headerRow, 'th', 'Name');
  addNodeWithText(headerRow, 'th', 'Pending');
  addNodeWithText(headerRow, 'th', 'Top Priority');
  addNodeWithText(headerRow, 'th', 'Active');
  addNodeWithText(headerRow, 'th', 'Idle');
  addNodeWithText(headerRow, 'th', 'Connect Jobs');
  addNodeWithText(headerRow, 'th', 'Backup Job');
  addNodeWithText(headerRow, 'th', 'Stalled');

  var tbody = addNode(table, 'tbody');
  for (var groupName in socketPool.groups) {
    var group = socketPool.groups[groupName];
    var tr = addNode(tbody, 'tr');

    addNodeWithText(tr, 'td', groupName);
    addNodeWithText(tr, 'td', group.pending_request_count);
    if (group.top_pending_priority != undefined)
      addNodeWithText(tr, 'td', group.top_pending_priority);
    else
      addNodeWithText(tr, 'td', '-');
    addNodeWithText(tr, 'td', group.active_socket_count);
    addNodeWithText(tr, 'td', group.idle_socket_count);
    addNodeWithText(tr, 'td', group.connect_job_count);
    addNodeWithText(tr, 'td', group.has_backup_job);
    addNodeWithText(tr, 'td', group.is_stalled);
  }
};

