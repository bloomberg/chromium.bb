// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set some global variables for the browsertest to pick up
var keyTestNamespace = {
  keypressSucceeded: false,
  keypressText: ''
};

/**
 * Method to make an XHR call to the server for status
 */
// TODO(chaitali): Update this method to poll and get status of all
// test vars in each poll and stop when all are true.
function poll() {

  var request = new XMLHttpRequest();
  request.open('GET', 'poll?test=keytest', true);

  request.onreadystatechange = function() {
    if (request.readyState == 4 && request.status == 200) {
      console.log('Polling status : ' + request.responseText);
      var data;
      try {
        data = JSON.parse(request.responseText);
      } catch (err) {
        console.log('Could not parse server response.');
        return;
      }
      // If keypress succeeded then
      // update relevant vars and stop polling.
      if (data.keypressSucceeded == true) {
        keyTestNamespace.keypressSucceeded = data.keypressSucceeded;
        keyTestNamespace.keypressText = data.keypressText;
      } else {
        // If keypress did not succeed we should
        // continue polling.
        setTimeout(poll, 1000);
      }
    }
  };

  request.onerror = function() {
    console.log('Polling failed');
  };

  request.send();
}

window.addEventListener(
    'load',
    poll.bind(null),
    false);

