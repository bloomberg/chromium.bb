// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mainWindow;
var debug = 0;
if (debug) {
  chrome.test.log = function(msg) {
    console.log(msg);
  }
}

function onReply(reply)  {
  switch (reply) {
    case 'enable':
      chrome.test.log('enabling intercept all keys');
      mainWindow.setInterceptAllKeys(true);
      break;
    case 'disable':
      chrome.test.log('disabling intercept all keys');
      mainWindow.setInterceptAllKeys(false);
      break;
  }
  chrome.test.sendMessage('readyForCommand', onReply);
}

function onKeyUp(e) {
  handleEvent(e);
  chrome.test.sendMessage('KeyReceived: '+ e.keyCode);
  return false;
}

// Based on keyevent browser test to convert event to string.
// Format used is:
// <type> <keyCode> <charCode> <ctrlKey> <shiftKey> <altKey> <commandKey>
// where <type> may be 'D' (keydown), 'P' (keypress) or 'U' (keyup).
// <ctrlKey>, <shiftKey> <altKey> and <commandKey> are boolean value indicating
// the state of corresponding modifier key.
function handleEvent(e) {
  var prefixes = {
    'keydown': 'D',
    'keypress': 'P',
    'keyup': 'U',
    'textInput': 'T',
  };

  var evt = e || window.event;
  var result = prefixes[evt.type] + ' ';
  if (evt.type == 'textInput') {
    result += evt.data;
  } else {
    // On Linux, the keydown event of a modifier key doesn't have the
    // corresponding modifier attribute set, while the keyup event does have,
    // eg. pressing and releasing ctrl may generate a keydown event with
    // ctrlKey=false and a keyup event with ctrlKey=true.
    // But Windows and Mac have opposite behavior than Linux.
    // To make the C++ testing code simpler, if it's a modifier key event,
    // then ignores the corresponding modifier attribute by setting it to true.
    var keyId = evt.keyIdentifier;
    result += (evt.keyCode + ' ' + evt.charCode + ' ' +
        (keyId == 'Control' ? true : evt.ctrlKey) + ' ' +
        (keyId == 'Shift' ? true : evt.shiftKey) + ' ' +
        (keyId == 'Alt' ? true : evt.altKey) + ' ' +
        (keyId == 'Meta' ? true : evt.metaKey));
  }

  chrome.test.log(result);
  chrome.test.sendMessage(result);
  return false;
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('main.html', {}, function(win) {
    mainWindow = win;
    win.contentWindow.document.addEventListener('keydown', handleEvent);
    win.contentWindow.document.addEventListener('keypress', handleEvent);
    win.contentWindow.document.addEventListener('keyup', onKeyUp);

    chrome.test.sendMessage('Launched');
    chrome.test.sendMessage('readyForCommand', onReply);
  });
});
