// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function sendResultToTest(data) {
  clients.matchAll({includeUncontrolled: true}).then(clients => {
    clients.forEach(client => {
      if (client.url.indexOf('payment_app_invocation.html') != -1) {
        client.postMessage(data);
      }
    });
  });
}

function getMessageFromPaymentAppWindow() {
  return new Promise((resolve, reject) => {
    var listener = self.addEventListener('message', e => {
      resolve(e.data);
      self.removeEventListener(listener);
    });
  });
}

self.addEventListener('paymentrequest', e => {
  var payment_app_window;

  // SW ------------------ openWindow() -----------------> payment_app_window
  // SW <---- postMessage('payment_app_window_ready') ---- payment_app_window
  // SW ------- postMessage('payment_app_request') ------> payment_app_window
  // SW <------ postMessage('payment_app_response') ------ payment_app_window
  e.waitUntil(clients.openWindow('payment_app_window.html')
                  .then(window_client => {
                    payment_app_window = window_client;
                    return getMessageFromPaymentAppWindow();
                  })
                  .then(message => {
                    sendResultToTest(message);
                    payment_app_window.postMessage('payment_app_request');
                    return getMessageFromPaymentAppWindow();
                  })
                  .then(message => { sendResultToTest(message); }));
});
