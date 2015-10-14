// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var buttonWasFocused = false;


function reset() {
  buttonWasFocused = false;
}

function onButtonReceivedFocus(e) {
  buttonWasFocused = true;
}

function onWindowMessage(e) {
  switch (e.data) {
    case 'verify':
      e.source.postMessage(
          buttonWasFocused ? 'was-focused' : 'was-not-focused', '*');
      break;
    case 'reset':
      reset();
      e.source.postMessage('', '*');
      break;
  }
}

function onLoad() {
  document.querySelector('button').onfocus = onButtonReceivedFocus;
}

window.addEventListener('message', onWindowMessage);
window.addEventListener('load', onLoad);
