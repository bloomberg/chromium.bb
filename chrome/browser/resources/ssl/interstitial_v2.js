// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expandedDetails = false;

function setupEvents() {
  var overridable = loadTimeData.getBoolean('overridable');

  $('primary-button').addEventListener('click', function() {
    if (overridable)
      sendCommand(CMD_DONT_PROCEED);
    else
      sendCommand(CMD_RELOAD);
  });

  if (overridable) {
    $('proceed-link').addEventListener('click', function(event) {
      sendCommand(CMD_PROCEED);
      event.preventDefault();
    });
  } else {
    $('help-link').addEventListener('click', function(event) {
      sendCommand(CMD_HELP);
      event.preventDefault();
    });

    $('error-code').textContent = loadTimeData.getString('errorCode');
    $('error-code').classList.remove('hidden');
  }

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
    event.preventDefault();
  });
}

document.addEventListener('DOMContentLoaded', setupEvents);
