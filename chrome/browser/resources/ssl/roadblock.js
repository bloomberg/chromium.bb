// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var showedMore = false;

function toggleMoreInfo(collapse) {
  $('more-info-long').hidden = collapse;
  $('more-info-short').hidden = !collapse;
  if (!collapse && !showedMore) {
    sendCommand(CMD_MORE);
    showedMore = true;
  }
}

// UI modifications and event listeners that take place after load.
function setupEvents() {
  $('proceed-button').hidden = false;
  $('proceed-button').addEventListener('click', function() {
    sendCommand(CMD_PROCEED);
  });

  if ($('more-info-title').textContent == '') {
    $('more-info-short').hidden = true;
    $('more-info-long').hidden = true;
    $('twisty-closed').style.display = 'none';
  } else {
    $('more-info-short').addEventListener('click', function() {
      toggleMoreInfo(false);
    });
    $('more-info-long').addEventListener('click', function() {
      toggleMoreInfo(true);
    });
  }

  $('exit-button').addEventListener('click', function() {
    sendCommand(CMD_DONT_PROCEED);
  });
}

document.addEventListener('DOMContentLoaded', setupEvents);
