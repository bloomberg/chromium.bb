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
function SocketsView(mainBoxId, socketPoolDivId, socketPoolGroupsDivId) {
  DivView.call(this, mainBoxId);

  g_browser.addSocketPoolInfoObserver(this);
  this.socketPoolDiv_ = document.getElementById(socketPoolDivId);
  this.socketPoolGroupsDiv_ = document.getElementById(socketPoolGroupsDivId);
}

inherits(SocketsView, DivView);

SocketsView.prototype.onSocketPoolInfoChanged = function(socketPoolInfo) {
  this.socketPoolDiv_.innerHTML = '';
  this.socketPoolGroupsDiv_.innerHTML = '';

  if (!socketPoolInfo)
    return;

  var socketPools = SocketPoolWrapper.createArrayFrom(socketPoolInfo);
  var tablePrinter = SocketPoolWrapper.createTablePrinter(socketPools);
  tablePrinter.toHTML(this.socketPoolDiv_, 'styledTable');

  // Add table for each socket pool with information on each of its groups.
  for (var i = 0; i < socketPools.length; ++i) {
    if (socketPools[i].origPool.groups != undefined) {
      var p = addNode(this.socketPoolGroupsDiv_, 'p');
      var br = addNode(p, 'br');
      var groupTablePrinter = socketPools[i].createGroupTablePrinter();
      groupTablePrinter.toHTML(p, 'styledTable');
    }
  }
};
