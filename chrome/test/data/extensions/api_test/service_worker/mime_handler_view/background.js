// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var seenPathsByServiceWorker = [];

// Called by mime_handler.js at the end of the test:
chrome.runtime.onMessage.addListener(function(msg) {
  chrome.test.assertEq('finish test by checking SW URLs', msg);
  chrome.test.assertEq([
    '/page_with_embed.html',
    // "/well-known-mime.ics" is loaded by page_with_embed.html, but it should
    // not have dispatched the "fetch" event in the Service Worker because it is
    // a plugin resource.
    '/mime_handler.html',
    '/mime_handler.js',
  ], seenPathsByServiceWorker, 'expected extension URLs');
  chrome.test.notifyPass();
});

navigator.serviceWorker.addEventListener('message', function(event) {
  seenPathsByServiceWorker.push(event.data.replace(location.origin, ''));
  event.ports[0].postMessage('ACK');
});

navigator.serviceWorker.register('sw.js').then(function() {
  return navigator.serviceWorker.ready;
}).then(function() {
  chrome.tabs.create({
    url: chrome.extension.getURL('page_with_embed.html'),
  });
}).catch(function(e) {
  chrome.test.fail('Unexpected error: ' + e);
});
