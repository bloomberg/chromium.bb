// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var resultQueue = new ResultQueue();
var pushRegistration = null;

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

// Queue storing asynchronous results received from the Service Worker. Results
// are sent to the test when requested.
function ResultQueue() {
  // Invariant: this.queue.length == 0 || this.pendingGets == 0
  this.queue = [];
  this.pendingGets = 0;
}

// Adds a data item to the queue. Will be sent to the test if there are
// pendingGets.
ResultQueue.prototype.push = function(data) {
  if (this.pendingGets > 0) {
    this.pendingGets--;
    sendResultToTest(data);
  } else {
    this.queue.unshift(data);
  }
};

// Called by native. Sends the next data item to the test if it is available.
// Otherwise increments pendingGets so it will be delivered when received.
ResultQueue.prototype.pop = function() {
  if (this.queue.length) {
    sendResultToTest(this.queue.pop());
  } else {
    this.pendingGets++;
  }
};

// Called by native. Immediately sends the next data item to the test if it is
// available, otherwise sends null.
ResultQueue.prototype.popImmediately = function() {
  sendResultToTest(this.queue.length ? this.queue.pop() : null);
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
    var registerMethodName =
        swRegistration.pushManager.register ? 'register' : 'subscribe';
    return swRegistration.pushManager[registerMethodName]()
        .then(function(registration) {
          pushRegistration = registration;
          sendResultToTest(registration.endpoint + ' - ' +
              (registration.registrationId || registration.subscriptionId));
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

function unregister() {
  if (!pushRegistration) {
    sendResultToTest('unregister error: no registration');
    return;
  }

  var unregisterMethodName =
      pushRegistration.unregister ? 'unregister' : 'unsubscribe';
  pushRegistration[unregisterMethodName]().then(function(result) {
    sendResultToTest('unregister result: ' + result);
  }, function(error) {
    sendResultToTest('unregister error: ' + error.name + ': ' + error.message);
  });
}

function hasRegistration() {
  navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.getSubscription();
  }).then(function(subscription) {
    sendResultToTest(subscription ? 'true - registered'
                                  : 'false - not registered');
  }).catch(sendErrorToTest);
}

addEventListener('message', function(event) {
  var message = JSON.parse(event.data);
  if (message.type == 'push')
    resultQueue.push(message.data);
}, false);
