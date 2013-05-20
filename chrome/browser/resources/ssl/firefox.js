// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Should match SSLBlockingPageCommands in ssl_blocking_page.cc.

(function() {

/** @const */ var CMD = {
  DONT_PROCEED: 0,
  PROCEED: 1,
  FOCUS: 2,
  MORE: 3,
  UNDERSTAND: 4
};

var showedMore = false;
var showedUnderstand = false;
var keyPressState = 0;
var gainFocus = false;

function $(o) {
  return document.getElementById(o);
}

function sendCommand(cmd) {
  window.domAutomationController.setAutomationId(1);
  window.domAutomationController.send(cmd);
}

function toggleMoreInfo() {
  var status = !$('more-info-content').hidden;
  $('more-info-content').hidden = status;
  if (status) {
    $('more-info-twisty-closed').hidden = !status;
    $('more-info-twisty-open').hidden = status;
  } else {
    $('more-info-twisty-open').hidden = status;
    $('more-info-twisty-closed').hidden = !status;
    if (!showedMore) {
      sendCommand(CMD.MORE);
      showedMore = true;
    }
  }
}

function toggleUnderstand() {
  var status = !$('understand-content').hidden;
  $('understand-content').hidden = status;
  if (status) {
    $('understand-twisty-closed').hidden = !status;
    $('understand-twisty-open').hidden = status;
  } else {
    $('understand-twisty-open').hidden = status;
    $('understand-twisty-closed').hidden = !status;
    if (!showedUnderstand) {
      sendCommand(CMD.UNDERSTAND);
      showedUnderstand = true;
    }
  }
}

// This allows errors to be skippped by typing "proceed" into the page.
function keyPressHandler(e) {
  var sequence = 'proceed';
  if (sequence.charCodeAt(keyPressState) == e.keyCode) {
    keyPressState++;
    if (keyPressState == sequence.length) {
      sendCommand(CMD.PROCEED);
      keyPressState = 0;
    }
  } else {
    keyPressState = 0;
  }
}

// Supports UMA timing, which starts after the warning is first viewed.
function handleFocusEvent() {
  if (!gainFocus) {
    sendCommand(CMD.FOCUS);
    gainFocus = true;
  }
}

// UI modifications and event listeners that take place after load.
function setupEvents() {
  if (templateData.errorType == 'overridable') {
    $('proceed').hidden = false;
    $('proceed-button').addEventListener('click', function() {
      sendCommand(CMD.PROCEED);
    });
  } else {
    document.addEventListener('keypress', keyPressHandler);
  }

  if (templateData.trialType == 'Condition18SSLNoImages') {
    $('icon-img').style.display = 'none';
  }

  $('exit-button').addEventListener('click', function() {
    sendCommand(CMD.DONT_PROCEED);
  });

  $('more-info-title').addEventListener('click', toggleMoreInfo);
  $('more-info-twisty-open').addEventListener('click', toggleMoreInfo);
  $('more-info-twisty-closed').addEventListener('click', toggleMoreInfo);
  $('understand-title').addEventListener('click', toggleUnderstand);
  $('understand-twisty-open').addEventListener('click', toggleUnderstand);
  $('understand-twisty-closed').addEventListener('click', toggleUnderstand);
}

window.addEventListener('focus', handleFocusEvent);
document.addEventListener('DOMContentLoaded', setupEvents);

}());

