// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Central user update listener triggered by a change in Chrome chat code.
 * @param {MessageEvent} event the user update event.
 */
function centralUserUpdate(event) {
  var centralRosterJid = event.data;
  chrome.extension.sendRequest(
    {
      msg: ChatBridgeEventTypes.CENTRAL_USER_UPDATE,
      jid: centralRosterJid
    }
  );
}

// Search for communication channel div.
var divRosterHandler = document.getElementById('roster_comm_link');
if (divRosterHandler) {
  divRosterHandler.addEventListener(ChatBridgeEventTypes.CENTRAL_USER_UPDATE,
      centralUserUpdate, false);
}
