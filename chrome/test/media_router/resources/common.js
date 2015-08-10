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
var joinedSession = null;
var presentationUrl = "http://www.google.com/#__testprovider__=true";
var startSessionRequest = new PresentationRequest(presentationUrl);

window.navigator.presentation.defaultRequest = startSessionRequest;

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
      sendResult(false, 'device unavailable');
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
    sendResult(false, 'Failed to start session');
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
 * Stops current session.
 */
function stopSession() {
  if (startedSession) {
    startedSession.close();
  }
  sendResult(true, '');
}


/**
 * Joins the started session and verify that it succeeds.
 * @param {!string} sessionId ID of session to join.
 */
function joinSession(sessionId) {
  var joinSessionRequest = new PresentationRequest(presentationUrl);
  joinSessionRequest.join(sessionId).then(function(session) {
    if (!session) {
      sendResult(false, 'joinSession returned null session');
    } else {
      joinedSession = session;
      sendResult(true, '');
    }
  }).catch(function(error) {
    sendResult(false, 'joinSession failed: ' + error.message);
  });
}

/**
 * Calls join() and verify that it fails.
 * @param {!string} sessionId ID of session to join.
 * @param {!string} expectedErrorMessage
 */
function joinSessionAndExpectFailure(sessionId, expectedErrorMessage) {
  var joinSessionRequest = new PresentationRequest(presentationUrl);
  joinSessionRequest.join(sessionId).then(function(session) {
    sendResult(false, 'join() unexpectedly succeeded.');
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
