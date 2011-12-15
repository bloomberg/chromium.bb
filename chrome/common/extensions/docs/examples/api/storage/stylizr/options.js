// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Store settings in the synchronized repository.
var storage = chrome.experimental.storage.sync;

// Get at the DOM controls used in the sample.
var resetButton = document.querySelector('button.reset');
var submitButton = document.querySelector('button.submit');
var textarea = document.querySelector('textarea');

// Load any CSS that may have previously been saved.
loadChanges();

submitButton.addEventListener('click', saveChanges);
resetButton.addEventListener('click', reset);

function saveChanges() {
  // Get the current CSS snippet from the form.
  var cssCode = textarea.value;
  // Check that there's some code there.
  if (!cssCode) {
    message('Error: No CSS specified');
    return;
  }
  // Save it locally (un-synchronized) using the Chrome extension storage API.
  storage.set({'css': cssCode}, function() {
    // Notify that we saved.
    message('Settings saved');
  });
}

function loadChanges() {
  storage.get('css', function(items) {
    if (items.css) {
      textarea.value = items.css;
      message('Loaded saved CSS.');
    }
  });
}

function reset() {
  // Remove the saved value from storage
  storage.remove('css', function(items) {
    message('Reset stored CSS');
  });
  // Refresh the text area.
  textarea.value = '';
}

function message(msg) {
  var message = document.querySelector('.message');
  message.innerText = msg;
  setTimeout(function() {
    message.innerText = '';
  }, 3000);
}
