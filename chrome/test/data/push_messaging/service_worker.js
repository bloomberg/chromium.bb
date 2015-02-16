// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The "onpush" event currently understands two values as message payload
// data coming from the test. Any other input is passed through to the
// document unchanged.
//
// "shownotification" - Display a Web Notification with event.waitUntil().
// "shownotification-without-waituntil"
//     - Display a Web Notification without using event.waitUntil().
this.onpush = function(event) {
  var data = event.data.text();
  if (!data.startsWith('shownotification')) {
    sendMessageToClients('push', data);
    return;
  }

  var result = registration.showNotification('Push test title', {
    body: 'Push test body',
    tag: 'push_test_tag'
  });

  if (data == 'shownotification-without-waituntil') {
    sendMessageToClients('push', 'immediate:' + data);
    return;
  }

  event.waitUntil(result.then(function() {
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
