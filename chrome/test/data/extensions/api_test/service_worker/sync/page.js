// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var getOnSyncWorkerPromise = function() {
  return new Promise(function(resolve, reject) {
    var serviceWorker;
    navigator.serviceWorker.register('sw.js').then(function() {
      // Wait until the service worker is active.
      return navigator.serviceWorker.ready;
    }).then(function(registration) {
      serviceWorker = registration.active;
      return registration.sync.register('send-chats');
    }).then(function() {
      resolve(serviceWorker);
    }).catch(function(err) {
      reject(err);
    });
  });
};

window.runServiceWorker = function() {
  getOnSyncWorkerPromise().then(function(serviceWorker) {
    var mc = new MessageChannel();
    // Called when ServiceWorker.onsync fires.
    mc.port1.onmessage = function(e) {
      if (e.data == 'connected') {
        window.domAutomationController.send('SERVICE_WORKER_READY');
        return;
      }
      if (e.data != 'SYNC: send-chats') {
        console.log('SW returned incorrect data: ' + e.data);
        chrome.test.sendMessage('FAIL');  // Fails the test fast.
        return;
      }
      chrome.test.sendMessage(e.data);
    };
    serviceWorker.postMessage('connect', [mc.port2]);
  }).catch(function(err) {
    window.domAutomationController.send(err);
  });
};
