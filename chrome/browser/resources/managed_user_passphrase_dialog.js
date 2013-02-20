// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function load() {
  $('unlock-passphrase-button').onclick = function(event) {
    chrome.send('checkPassphrase', [$('passphrase-entry').value]);
  };
  $('cancel-passphrase-button').onclick = function(event) {
    // TODO(akuegel): Replace by closeDialog.
    chrome.send('DialogClose');
  };
  $('passphrase-entry').oninput = function(event) {
    $('unlock-passphrase-button').disabled = $('passphrase-entry').value == '';
    $('incorrect-passphrase-warning').hidden = true;
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
