// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onpush = function(event) {
  // TODO(peter): Remove this check once Blink supports PushMessageData.json().
  var data = event.data;
  if (typeof data !== 'string')
    data = data.text();

  if (data !== 'shownotification') {
    sendMessageToClients('push', data);
    return;
  }

  // TODO(peter): Switch to self.registration.showNotification once implemented.
  event.waitUntil(showLegacyNonPersistentNotification('Push test title', {
    body: 'Push test body',
    tag: 'push_test_tag'
  }).then(function(notification) {
    sendMessageToClients('push', data);
  }, function(ex) {
    sendMessageToClients('push', String(ex));
  }));
};

function sendMessageToClients(type, data) {
  var message = JSON.stringify({
    'type': type,
    'data': data
  });
  clients.getAll().then(function(clients) {
    clients.forEach(function(client) {
      client.postMessage(message);
    });
  }, function(error) {
    console.log(error);
  });
}

function showLegacyNonPersistentNotification(title, options) {
  return new Promise(function(resolve, reject) {
    var notification = new Notification(title, options);
    notification.onshow = function() { resolve(notification); };
    notification.onerror = function() { reject(new Error('Failed to show')); };
  });
}