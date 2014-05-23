// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;
var inputElement;
var waitingForBlur = false;
var seenBlurAfterFocus = false;

var LOG = function(msg) {
  window.console.log(msg);
};

var sendMessage = function(data) {
  embedder.postMessage(JSON.stringify(data), '*');
};

var waitForBlurAfterFocus = function() {
  LOG('seenBlurAfterFocus: ' + seenBlurAfterFocus);
  if (seenBlurAfterFocus) {
    // Already seen it.
    sendMessage(['response-seenBlurAfterFocus']);
    return;
  }

  // Otherwise we will wait.
  waitingForBlur = true;
};

var waitForFocus = function() {
  inputElement = document.createElement('input');
  inputElement.addEventListener('focus', function(e) {
    LOG('input.focus');
    sendMessage(['response-seenFocus']);

    var blurHandler = function(e) {
      seenBlurAfterFocus = true;
      if (waitingForBlur) {
        inputElement.removeEventListener('blur', blurHandler);
        sendMessage(['response-seenBlurAfterFocus']);
      }
    };
    inputElement.addEventListener('blur', blurHandler);
  });
  document.body.appendChild(inputElement);

  inputElement.focus();
};

window.addEventListener('message', function(e) {
  var data = JSON.parse(e.data);
  if (data[0] == 'connect') {
    embedder = e.source;
    sendMessage(['connected']);
  } else if (data[0] == 'request-hasFocus') {
    var hasFocus = document.hasFocus();
    sendMessage(['response-hasFocus', hasFocus]);
  } else if (data[0] == 'request-waitForFocus') {
    waitForFocus();
  } else if (data[0] == 'request-waitForBlurAfterFocus') {
    waitForBlurAfterFocus();
  }
});

window.addEventListener('focus', function(e) {
  sendMessage(['focused']);
});

window.addEventListener('blur', function(e) {
  sendMessage(['blurred']);
});
