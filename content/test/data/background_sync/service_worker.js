// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The "onsync" event currently understands commands (passed as
// registration tags) coming from the test. Any other tag is
// passed through to the document unchanged.
//
// "delay" - Delays finishing the sync event with event.waitUntil.
//           Send a postMessage of "completeDelayedOneShot" to finish the
//           event.

'use strict';

var resolveCallback = null;
var rejectCallback = null;

this.onmessage = function(event) {
  if (event.data['action'] === 'completeDelayedOneShot') {
    if (resolveCallback === null) {
      sendMessageToClients('sync', 'error - resolveCallback is null');
      return;
    }

    resolveCallback();
    sendMessageToClients('sync', 'ok - delay completed');
    return;
  }

  if (event.data['action'] === 'rejectDelayedOneShot') {
    if (rejectCallback === null) {
      sendMessageToClients('sync', 'error - rejectCallback is null');
      return;
    }

    rejectCallback();
    sendMessageToClients('sync', 'ok - delay rejected');
  }

  if (event.data['action'] === 'registerOneShot') {
    var tag = event.data['tag'];
    registration.sync.register(tag)
      .then(function () {
        sendMessageToClients('register', 'ok - ' + tag + ' registered in SW');
      })
      .catch(sendSyncErrorToClients);
  }

  if (event.data['action'] === 'getRegistrationOneShot') {
    var tag = event.data['tag'];
    registration.sync.getTags(tag)
      .then(function(tags) {
        if (tags.indexOf(tag) >= 0) {
          sendMessageToClients('register', 'ok - ' + tag + ' found');
        } else {
          sendMessageToClients('register', 'error - ' + tag + ' not found');
          return;
        }
      })
      .catch(sendSyncErrorToClients);
  }

  if (event.data['action'] === 'getRegistrationsOneShot') {
    registration.sync.getTags()
      .then(function(tags) {
        sendMessageToClients('register', 'ok - ' + tags.toString());
      })
      .catch(sendSyncErrorToClients);
  }
}

this.onsync = function(event) {
  var eventProperties = [
    // Extract name from toString result: "[object <Class>]"
    Object.prototype.toString.call(event).match(/\s([a-zA-Z]+)/)[1],
    (typeof event.waitUntil)
  ];

  if (eventProperties[0] != 'SyncEvent') {
    sendMessageToClients('sync', 'error - wrong event type');
    return;
  }

  if (eventProperties[1] != 'function') {
    sendMessageToClients('sync', 'error - wrong wait until type');
  }

  if (event.tag === undefined) {
    sendMessageToClients('sync', 'error - registration missing tag');
    return;
  }

  var tag = event.tag;

  if (tag === 'delay') {
    var syncPromise = new Promise(function(resolve, reject) {
      resolveCallback = resolve;
      rejectCallback = reject;
    });
    event.waitUntil(syncPromise);
    return;
  }

  sendMessageToClients('sync', tag + ' fired');
};

function sendMessageToClients(type, data) {
  clients.matchAll().then(function(clients) {
    clients.forEach(function(client) {
      client.postMessage({type, data});
    });
  }, function(error) {
    console.log(error);
  });
}

function sendSyncErrorToClients(error) {
  sendMessageToClients('sync', error.name + ' - ' + error.message);
}
