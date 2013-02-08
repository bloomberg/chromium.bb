// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Returns an array of the files currently selected in the file manager.
function getSelectedFiles() {
  var rows = document.getElementById('detail-table').getElementsByTagName('li');
  var selected = [];
  for (var i = 0; i < rows.length; ++i) {
    if (rows[i].hasAttribute('selected')) {
      selected.push(
          rows[i].getElementsByClassName('filename-label')[0].textContent);
    }
  }
  return selected;
}

// Sends a fake key event with the given |keyIdentifier| and optional |ctrl|
// modifier to the file manager.
function fakeKeyDown(keyIdentifier, ctrl) {
  var event = document.createEvent('KeyboardEvent');
  event.initKeyboardEvent(
      'keydown',
      true,  // canBubble
      true,  // cancelable
      window,
      keyIdentifier,
      0,  // keyLocation
      ctrl,
      false,  // alt
      false,  // shift
      false);  // meta
  var detailTable = document.getElementById('detail-table')
  detailTable.getElementsByTagName('list')[0].dispatchEvent(event);
}

// Fakes pressing the down arrow until the given |filename| is selected.
function selectFile(filename) {
  for (var tries = 0; tries < 10; ++tries) {
    fakeKeyDown('Down', false);
    var selection = getSelectedFiles();
    if (selection.length === 1 && selection[0] === filename)
      return true;
  }
  console.log('Failed to select file "' + filename + '"');
  return false;
}

// Selects |filename| and fakes pressing Ctrl+C Ctrl+V (copy, paste).
function fakeFileCopy(filename) {
  if (!selectFile(filename))
    return false;
  fakeKeyDown('U+0043', true);  // Ctrl+C
  fakeKeyDown('U+0056', true);  // Ctrl+V
  return true;
}

// Selects |filename| and fakes pressing Delete.
function fakeFileDelete(filename) {
  if (!selectFile(filename))
    return false;
  fakeKeyDown('U+007F', false);  // Delete
  return true;
}
