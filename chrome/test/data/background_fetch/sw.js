// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Service Worker initialization listeners.
self.addEventListener('install', e => e.waitUntil(skipWaiting()));
self.addEventListener('activate', e => e.waitUntil(clients.claim()));

// Posts |msg| to background_fetch.js.
function postToWindowClients(msg) {
  clients.matchAll({ type: 'window' }).then(clientWindows => {
    for (const client of clientWindows) client.postMessage(msg);
  });
}

// Background Fetch event listeners.
self.addEventListener('backgroundfetchsuccess', e => {
  e.waitUntil(e.updateUI({title: 'New Fetched Title!'}).then(
      () => postToWindowClients(e.type)));
});

self.addEventListener('backgroundfetchfail', e => {
  e.waitUntil(e.updateUI({title: 'New Failed Title!'}).then(
      () => postToWindowClients(e.type)));
});