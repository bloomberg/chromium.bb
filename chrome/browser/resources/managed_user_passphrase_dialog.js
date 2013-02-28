// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function load() {
  var checkPassphrase = function(event) {
    chrome.send('checkPassphrase', [$('passphrase-entry').value]);
  };
  var closeDialog = function(event) {
    // TODO(akuegel): Replace by closeDialog.
    chrome.send('DialogClose');
  };
  // Directly set the focus on the input box so the user can start typing right
  // away.
  $('passphrase-entry').focus();
  $('unlock-passphrase-button').onclick = checkPassphrase;
  $('cancel-passphrase-button').onclick = closeDialog;
  $('passphrase-entry').oninput = function(event) {
    $('unlock-passphrase-button').disabled = $('passphrase-entry').value == '';
    $('incorrect-passphrase-warning').hidden = true;
  };
  $('passphrase-entry').onkeypress = function(event) {
    if (!event)
      return;
    // Check if the user pressed enter.
    if (event.keyCode == 13)
      checkPassphrase(event);
  };

  // Pressing escape anywhere in the frame should work.
  document.onkeyup = function(event) {
    if (!event)
      return;
    // Check if the user pressed escape.
    if (event.keyCode == 27)
      closeDialog(event);
  };
}

function passphraseCorrect() {
  chrome.send('DialogClose', ['true']);
}

function passphraseIncorrect() {
  $('incorrect-passphrase-warning').hidden = false;
  $('passphrase-entry').focus();
}

window.addEventListener('DOMContentLoaded', load);
