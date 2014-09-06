// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;
var inputElement1;
var inputElement2;
var waitingForBlur = false;

var LOG = function(msg) {
  window.console.log(msg);
};

var sendMessage = function(data) {
  embedder.postMessage(JSON.stringify(data), '*');
};

var onInputState = {fired: false, value: ''};
// Waits for oninput event to fire on |inputElement1|.
// Upon receiving the event, we ping back the embedder with the value of
// the textbox.
var waitForOnInput = function() {
  var inputElement1 = document.querySelector('input');
  LOG('inputElement1: ' + inputElement1);
  if (onInputState.fired) {
    sendMessage(['response-waitForOnInput', onInputState.value]);

    onInputState.fired = false;
    if (inputElement1.value == 'InputTest456') {
      // Prepare for next step, step 3.
      inputElement1.value = '';
    }
  }
};

// Waits for the first textbox element to be focused.
var waitForFocus = function() {
  inputElement1 = document.createElement('input');
  inputElement1.id = 'input1';
  inputElement1.oninput = function() {
    LOG('inputElement1.oninput: ' + inputElement1.value);
    onInputState.fired = true;
    onInputState.value = inputElement1.value;
  };
  var inputElement1FocusListener = function() {
    LOG('inputElement1.focus');
    inputElement1.removeEventListener('focus', inputElement1FocusListener);
    sendMessage(['response-seenFocus']);
  };
  inputElement1.addEventListener('focus', inputElement1FocusListener);
  document.body.appendChild(inputElement1);

  inputElement2 = document.createElement('input');
  inputElement2.id = 'input2';
  document.body.appendChild(inputElement2);

  inputElement1.focus();
};

// Waits for the first <input> element to be focused again.
var waitForFocusAgain = function() {
  var focusAgainListener = function(e) {
    LOG('inputElement1.focus again');
    inputElement1.removeEventListener('focus', focusAgainListener);
    sendMessage(['response-focus-again']);
  };
  inputElement1.addEventListener('focus', focusAgainListener);
  inputElement1.focus();
};

window.addEventListener('message', function(e) {
  var data = JSON.parse(e.data);
  if (data[0] == 'connect') {
    embedder = e.source;
    sendMessage(['connected']);
  } else if (data[0] == 'request-waitForFocus') {
    waitForFocus();
  } else if (data[0] == 'request-waitForOnInput') {
    waitForOnInput();
  } else if (data[0] == 'request-moveFocus') {
    var onInputElement2Focused = function() {
      LOG('inputElement2.focus');
      inputElement2.removeEventListener('focus', onInputElement2Focused);
      sendMessage(['response-movedFocus', inputElement1.value]);

      // Prepare for next step.
      // Set 'InputTestABC' in inputElement1 and put caret at 6 (after 'T').
      inputElement1.focus();
      inputElement1.value = 'InputTestABC';
      inputElement1.selectionStart = 6;
      inputElement1.selectionEnd = 6;
    };
    inputElement2.addEventListener('focus', onInputElement2Focused);

    inputElement2.focus();
  } else if (data[0] == 'request-valueAfterExtendSelection') {
    sendMessage(['response-valueAfterExtendSelection', inputElement1.value]);
  } else if (data[0] == 'request-focus-again') {
    waitForFocusAgain();
  }
});
