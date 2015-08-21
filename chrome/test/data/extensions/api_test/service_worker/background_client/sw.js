// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var background_url;

self.addEventListener('install', function(event) {
  chrome.getBackgroundClient().then(function(client) {
    background_url = client.url;
  });
});

self.addEventListener('fetch', function(event) {
  event.respondWith(new Response(background_url));
});
