// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expandedDetails = false;
var keyPressState = 0;

/*
 * A convenience method for sending commands to the parent page.
 * @param {string} cmd  The command to send.
 */
function sendCommand(cmd) {
  window.domAutomationController.setAutomationId(1);
  window.domAutomationController.send(cmd);
}

function setupEvents() {
  $('primary-button').addEventListener('click', function() {
     sendCommand(DRP_CMD_TAKE_ME_BACK);
  });

  $('secondary-button').addEventListener('click', function() {
     sendCommand(DRP_CMD_PROCEED);
  });

  preventDefaultOnPoundLinkClicks();
}

document.addEventListener('DOMContentLoaded', setupEvents);
