// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onpush = function(event) {
  sendMessageToClients('push', event.data);
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
