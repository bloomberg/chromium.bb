// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expandedDetails = false;

function setupEvents() {
  $('safety-button').addEventListener('click', function() {
    sendCommand(CMD_DONT_PROCEED);
  });

  $('proceed-link').addEventListener('click', function(event) {
    sendCommand(CMD_PROCEED);
    event.preventDefault();  // Don't let the fragment navigate.
  });

  $('details-button').addEventListener('click', function(event) {
    var hiddenDetails = $('details').classList.toggle('hidden');
    $('details-button').innerText = hiddenDetails ?
        loadTimeData.getString('openDetails') :
        loadTimeData.getString('closeDetails');
    if (!expandedDetails) {
      // Record a histogram entry only the first time that details is opened.
      sendCommand(CMD_MORE);
      expandedDetails = true;
    }
    event.preventDefault();  // Don't let the fragment navigate.
  });
}

document.addEventListener('DOMContentLoaded', setupEvents);
