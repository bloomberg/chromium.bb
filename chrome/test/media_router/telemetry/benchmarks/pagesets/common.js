/**
 * Copyright 2016 The Chromium Authors. All rights reserved.
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
        console.log('device available');
      } else {
        availability.onchange = function(newAvailability) {
          if (newAvailability)
            console.log('got new availability');
        }
      }
  }).catch(function(){
      console.log('error');
  });
}

/**
 * Starts session.
 */
function startSession() {
  startSessionPromise = startSessionRequest.start();
  console.log('start session');
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
        console.log('Failed to start session');
      } else {
        // set the new session
        startedSession = session;
        console.log('Session has been started');
      }
    }).catch(function() {
      // terminate old session if exists
      terminateSession();
      console.log('Failed to start session');
    })
  }
}

/**
 * Terminates current session.
 */
function terminateSession() {
  if (startedSession) {
    startedSession.terminate();
  }
}

