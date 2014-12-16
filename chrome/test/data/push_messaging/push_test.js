// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var pushData = new FutureData();

// Sends data back to the test. This must be in response to an earlier
// request, but it's ok to respond asynchronously. The request blocks until
// the response is sent.
function sendResultToTest(result) {
  console.log('sendResultToTest: ' + result);
  if (window.domAutomationController) {
    domAutomationController.send('' + result);
  }
}

function sendErrorToTest(error) {
  sendResultToTest(error.name + ' - ' + error.message);
}

// A container for a single piece of data. The data does not have to be
// available yet when the getter is called, as all responses to the test are
// asynchronous.
function FutureData() {
  this.data = null;
  this.waiting = false;
}

// Sends the data to the test if it is available. Otherwise sets the
// |waiting| flag.
FutureData.prototype.get = function() {
  if (this.data) {
    sendResultToTest(this.data);
  } else {
    this.waiting = true;
  }
};

// Sets a new data value. If the |waiting| flag is on, it is turned off and
// the data is sent to the test.
FutureData.prototype.set = function(data) {
  this.data = data;
  if (this.waiting) {
    sendResultToTest(data);
    this.waiting = false;
  }
};

FutureData.prototype.getImmediately = function() {
  sendResultToTest(this.data);
};

// Notification permission has been coalesced with Push permission. After
// this is granted, Push API registration can succeed.
function requestNotificationPermission() {
  Notification.requestPermission(function(permission) {
    sendResultToTest('permission status - ' + permission);
  });
}

function registerServiceWorker() {
  // The base dir used to resolve service_worker.js and the scope depends on
  // whether this script is included from an html file in ./, subscope1/, or
  // subscope2/.
  navigator.serviceWorker.register('service_worker.js', {scope: './'}).then(
      function(swRegistration) {
        sendResultToTest('ok - service worker registered');
      }, sendErrorToTest);
}

function unregisterServiceWorker() {
  navigator.serviceWorker.getRegistration().then(function(swRegistration) {
    swRegistration.unregister().then(function(result) {
      sendResultToTest('service worker unregistration status: ' + result);
    })
  }).catch(sendErrorToTest);
}

function removeManifest() {
  var element = document.querySelector('link[rel="manifest"]');
  if (element) {
    element.parentNode.removeChild(element);
    sendResultToTest('manifest removed');
  } else {
    sendResultToTest('unable to find manifest element');
  }
}

function registerPush() {
  navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.register()
        .then(function(pushRegistration) {
          sendResultToTest(pushRegistration.endpoint + ' - ' +
                           pushRegistration.registrationId);
        });
  }).catch(sendErrorToTest);
}

function hasPermission() {
  navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.hasPermission()
        .then(function(permission) {
          sendResultToTest('permission status - ' + permission);
        });
  }).catch(sendErrorToTest);
}

function isControlled() {
  if (navigator.serviceWorker.controller) {
    sendResultToTest('true - is controlled');
  } else {
    sendResultToTest('false - is not controlled');
  }
}

addEventListener('message', function(event) {
  var message = JSON.parse(event.data);
  if (message.type == 'push') {
    pushData.set(message.data);
  }
}, false);
