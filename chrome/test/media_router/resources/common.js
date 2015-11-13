/**
 * Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Common APIs for presentation integration tests.
 *
 */

var startSessionPromise = null;
var startedSession = null;
var reconnectedSession = null;
var presentationUrl = "http://www.google.com/#__testprovider__=true";
var startSessionRequest = new PresentationRequest(presentationUrl);
var defaultRequestSessionId = null;

window.navigator.presentation.defaultRequest = startSessionRequest;
window.navigator.presentation.defaultRequest.onconnectionavailable = function(e)
{
  defaultRequestSessionId = e.connection.id;
};

/**
 * Waits until one device is available.
 */
function waitUntilDeviceAvailable() {
  startSessionRequest.getAvailability(presentationUrl).then(
  function(availability) {
    console.log('availability ' + availability.value + '\n');
    if (availability.value) {
      sendResult(true, '');
    } else {
      availability.onchange = function(newAvailability) {
        if (newAvailability)
          sendResult(true, '');
      }
    }
  }).catch(function(){
    sendResult(false, 'got error');
  });
}

/**
 * Starts session.
 */
function startSession() {
  startSessionPromise = startSessionRequest.start();
  console.log('start session');
  sendResult(true, '');
}

/**
 * Checks if the session has been started successfully.
 */
function checkSession() {
  if (!startSessionPromise) {
    sendResult(false, 'Did not attempt to start session');
  } else {
    startSessionPromise.then(function(session) {
      if(!session) {
        sendResult(false, 'Failed to start session');
      } else {
        // set the new session
        startedSession = session;
        sendResult(true, '');
      }
    }).catch(function() {
      // close old session if exists
      startedSession && startedSession.close();
      sendResult(false, 'Failed to start session');
    })
  }
}

/**
 * Checks the start() request fails with expected error and message substring.
 * @param {!string} expectedErrorName
 * @param {!string} expectedErrorMessageSubstring
 */
function checkStartFailed(expectedErrorName, expectedErrorMessageSubstring) {
  if (!startSessionPromise) {
    sendResult(false, 'Did not attempt to start session');
  } else {
    startSessionPromise.then(function(session) {
      sendResult(false, 'start() unexpectedly succeeded.');
    }).catch(function(e) {
      if (expectedErrorName != e.name) {
        sendResult(false, 'Got unexpected error: ' + e.name);
      }
      if (e.message.indexOf(expectedErrorMessageSubstring) == -1) {
        sendResult(false,
          'Error message is not correct, it should contain "' +
          expectedErrorMessageSubstring + '"');
      }
      sendResult(true, '');
    })
  }
}

/**
 * Stops current session.
 */
function stopSession() {
  if (startedSession) {
    startedSession.close();
  }
  sendResult(true, '');
}


/**
 * Reconnects to |sessionId| and verifies that it succeeds.
 * @param {!string} sessionId ID of session to reconnect.
 */
function reconnectSession(sessionId) {
  var reconnectSessionRequest = new PresentationRequest(presentationUrl);
  reconnectSessionRequest.reconnect(sessionId).then(function(session) {
    if (!session) {
      sendResult(false, 'reconnectSession returned null session');
    } else {
      reconnectedSession = session;
      sendResult(true, '');
    }
  }).catch(function(error) {
    sendResult(false, 'reconnectSession failed: ' + error.message);
  });
}

/**
 * Calls reconnect(sessionId) and verifies that it fails.
 * @param {!string} sessionId ID of session to reconnect.
 * @param {!string} expectedErrorMessage
 */
function reconnectSessionAndExpectFailure(sessionId, expectedErrorMessage) {
  var reconnectSessionRequest = new PresentationRequest(presentationUrl);
  reconnectSessionRequest.reconnect(sessionId).then(function(session) {
    sendResult(false, 'reconnect() unexpectedly succeeded.');
  }).catch(function(error) {
    if (error.message.indexOf(expectedErrorMessage) > -1) {
      sendResult(true, '');
    } else {
      sendResult(false,
        'Error message mismatch. Expected: ' + expectedErrorMessage +
        ', actual: ' + error.message);
    }
  });
}

/**
 * Sends the test result back to browser test.
 * @param passed true if test passes, otherwise false.
 * @param errorMessage empty string if test passes, error message if test
 *                      fails.
 */
function sendResult(passed, errorMessage) {
  window.domAutomationController.send(JSON.stringify({
    passed: passed,
    errorMessage: errorMessage
  }));
}
