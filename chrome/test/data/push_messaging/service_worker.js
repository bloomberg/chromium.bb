// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onpush = function(event) {
  var data = event.data.text();
  if (data !== 'shownotification') {
    sendMessageToClients('push', data);
    return;
  }

  event.waitUntil(registration.showNotification('Push test title', {
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
