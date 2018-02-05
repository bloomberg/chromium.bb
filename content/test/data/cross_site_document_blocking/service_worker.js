// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This service worker is used by the CrossSiteDocumentBlockingServiceWorkerTest
// browser test - please see the comments there for more details.

self.addEventListener('activate', function(event) {
  event.waitUntil(self.clients.claim()); // Become available to all pages
});

function createHtmlNoSniffResponse() {
  var headers = new Headers();
  headers.append('Content-Type', 'text/html');
  headers.append('X-Content-Type-Options', 'nosniff');
  return new Response('Response created by service worker',
                      { status: 200, headers: headers });
}

var previousResponse = undefined;
var previousUrl = undefined;

self.addEventListener('fetch', function(event) {
  // This handles response to the request issued in the
  // CrossSiteDocumentBlockingServiceWorkerTest.NoNetwork test.
  if (event.request.url.endsWith('data_from_service_worker')) {
    event.respondWith(createHtmlNoSniffResponse());
    return;
  }

  // This handles response to the first request in the
  // CrossSiteDocumentBlockingServiceWorkerTest.NetworkAndOpaqueResponse test.
  if (previousUrl != event.request.url || !previousResponse) {
    event.respondWith(new Promise(function(resolve, reject) {
        fetch(event.request)
            .then(function(response) {
                previousResponse = response;
                previousUrl = event.request.url;

                reject('Expected error from service worker');
            })
            .catch(function(error) {
                reject('Unexpected error: ' + error);
            });
    }));
    return;
  }

  // This handles response to the second request in the
  // CrossSiteDocumentBlockingServiceWorkerTest.NetworkAndOpaqueResponse test.
  if (previousUrl == event.request.url && previousResponse) {
    event.respondWith(previousResponse.clone());
    return;
  }
});
