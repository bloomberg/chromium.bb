// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Keypress handler for the textarea. Sends the textarea value
 * to the HTTP server when "Enter" key is pressed.
 * @param {Event} event The keypress event.
 */
function handleTextareaKeyPressed(event) {
  // If the "Enter" key is pressed then process the text in the textarea.
  if (event.which == 13) {
    var testTextVal = document.getElementById('testtext').value;
    var postParams = 'text=' + testTextVal;

    var request = new XMLHttpRequest();
    request.open('POST', 'keytest/test', true);
    request.setRequestHeader(
        'Content-type', 'application/x-www-form-urlencoded');

    request.onreadystatechange = function() {
      if (request.readyState == 4 && request.status == 200) {
        console.log('Sent POST request to server.');
      }
    };

    request.onerror = function() {
      console.log('Request failed');
    };

    request.send(postParams);
  }
}

window.addEventListener(
    'load',
    function() {
      document.getElementById('testtext').addEventListener(
        'keypress',
        handleTextareaKeyPressed,
        false);
    },
    false);

