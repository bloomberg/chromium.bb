/**
 * Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Common APIs for presentation integration tests.
 *
 */

var startSessionPromise = null;
var currentSession = null;
var presentation = window.navigator.presentation;
var presUrl = "http://www.google.com/#__testprovider__=true";

/**
 * Waits until one device is available.
 */
function waitUntilDeviceAvailable() {
  presentation.getAvailability(presUrl).then(function(availability) {
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
  var presId = Math.random().toFixed(6).substr(2);
  startSessionPromise = presentation.startSession(presUrl, presId);
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
    startSessionPromise.then(function (currentSession) {
      if(!currentSession) {
        sendResult(false, 'Failed to start session');
      } else {
        // set the new session
        currentSession = currentSession;
        sendResult(true, '');
      }
    }).catch(function() {
      // close old session if exists
      currentSession && currentSession.close();
      sendResult(false, 'Failed to start session');
    })
  }
}


/**
 * Stops current session.
 */
function stopSession() {
  if (currentSession) {
    currentSession.close();
  }
  sendResult(true, '');
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
